// MIT License - see LICENSE
//
// Nt Test tested with itself: Good uses the API the way a test would and must pass. The other
// fixtures fail or skip on purpose, so main() runs them with QTest::qExec() and checks the logs.
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTestEventLoop>
#include <QTimer>

#include <cstdio>
#include <stdexcept>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace geometry {

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point &o) const { return x == o.x && y == o.y; }
};

// Found by argument-dependent lookup.
char *toString(const Point &p);
char *toString(const Point &p) {
    return qstrdup((QByteArray::number(p.x) + "," + QByteArray::number(p.y)).constData());
}

} // namespace geometry

struct Opaque {
    bool operator==(const Opaque &) const { return false; }
};

struct Specialized {
    bool operator==(const Specialized &) const { return false; }
};

template <> char *QTest::toString(const Specialized &) { return qstrdup("specialized"); }

enum class Color { Red = 1, Green = 2 };

class Emitter : public QObject {
    Q_OBJECT
public:
    void fire(int value, const QString &text) { emit fired(value, text); }
    void tick() { emit ticked(); }
    void paint(Color color) { emit painted(color); }

signals:
    void fired(int value, const QString &text);
    void ticked();
    void painted(Color color);
};

// The order of calls is checked at the end of the run, in cleanupTestCase().
class Good : public QObject {
    Q_OBJECT

public:
    bool cleanedUp = false;

private slots:
    void initTestCase() { m_calls += "start "; }
    void init() { ++m_inits; }
    void cleanup() { ++m_cleanups; }

    void cleanupTestCase() {
        QVERIFY(m_calls.startsWith("start "));
        QCOMPARE(m_inits, m_cleanups);
        cleanedUp = true;
    }

    void verify() {
        QVERIFY(true);
        QVERIFY2(1 + 1 == 2, "arithmetic");
        QCOMPARE(QTest::currentTestFunction(), "verify");
        QVERIFY(!QTest::currentDataTag());
        QVERIFY(!QTest::currentTestFailed());
    }

    void compareMixedTypes() {
        QCOMPARE(QList<int>({1, 2}).size(), 2);
        QCOMPARE(static_cast<unsigned>(3), 3);
        QCOMPARE(QString("abc"), "abc");
        QCOMPARE(QByteArray("abc"), QByteArray("abc"));
        const char *text = "abc";
        QCOMPARE(text, QByteArray("abc").constData());
        QObject *none = nullptr;
        QCOMPARE(none, nullptr);
        QCOMPARE(Color::Red, Color::Red);
    }

    void compareFloats() {
        QCOMPARE(0.1 + 0.2, 0.3);
        QCOMPARE(0.1F + 0.2F, 0.3F);
        QCOMPARE(1.0, 1);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(nan, nan);
        QCOMPARE(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    }

    void compareContainers() {
        QCOMPARE(QList<int>({1, 2}), QList<int>({1, 2}));
        QCOMPARE(QList<QString>({"a"}), QList<QString>({"a"}));
        QCOMPARE(QStringList({"a"}), QStringList({"a"}));
        QCOMPARE(geometry::Point({1, 2}), geometry::Point({1, 2}));
    }

    void compareOperators() {
        QCOMPARE_EQ(1, 1);
        QCOMPARE_NE(1, 2);
        QCOMPARE_LT(1, 2);
        QCOMPARE_LE(2, 2);
        QCOMPARE_GT(2u, 1);
        QCOMPARE_GE(-1, -1);
        // -1 converted to unsigned would be greater than everything.
        QCOMPARE_LT(-1, 1u);
    }

    void data_data() {
        QTest::addColumn<int>("number");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<const char *>("raw");

        QTest::newRow("first") << 1 << QString("one") << QByteArray("x") << "r1";
        QTest::newRow("literals") << 2 << "two" << "y" << "r2";
        for (int i = 3; i < 5; ++i)
            QTest::addRow("row-%d", i) << i << "many" << "z" << "r3";
    }

    void data() {
        QFETCH(int, number);
        QFETCH(QString, text);
        QFETCH(QByteArray, bytes);
        QFETCH(const char *, raw);
        QVERIFY(number >= 1);
        QVERIFY(!text.isEmpty());
        QVERIFY(!bytes.isEmpty());
        QCOMPARE(raw[0], 'r');
        QVERIFY(QTest::currentDataTag());
        QTEST(number, "number");
    }

    void spy() {
        Emitter emitter;
        QSignalSpy spy(&emitter, &Emitter::fired);
        QVERIFY(spy.isValid());
        emitter.fire(7, "seven");
        emitter.fire(8, "eight");
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).size(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), 7);
        QCOMPARE(spy.at(0).at(1).toString(), "seven");
        const QList<QVariant> last = spy.takeLast();
        QCOMPARE(last.at(0).value<int>(), 8);
        QCOMPARE(last.at(0).toDouble(), 8.0);
        QCOMPARE(last.at(0).toLongLong(), 8);
        QVERIFY(last.at(0).toBool());
        QVERIFY(last.at(0).canConvert<long>());
        QCOMPARE(last.at(0).typeId(), QMetaType::Int);
        QCOMPARE(last.at(1).toInt(), 0);
        QVERIFY(last.at(1).canConvert<QString>());
    }

    void spyWithoutArguments() {
        Emitter emitter;
        QSignalSpy spy(&emitter, &Emitter::ticked);
        emitter.tick();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().isEmpty());
    }

    void spyEnumArgument() {
        Emitter emitter;
        QSignalSpy spy(&emitter, &Emitter::painted);
        emitter.paint(Color::Green);
        QCOMPARE(spy.at(0).at(0).value<Color>(), Color::Green);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }

    void spyOnNull() {
        const Emitter *none = nullptr;
        QTest::ignoreMessage(QtWarningMsg, "QSignalSpy: cannot spy on a null object");
        QSignalSpy spy(none, &Emitter::ticked);
        QVERIFY(!spy.isValid());
    }

    void spyWait() {
        Emitter emitter;
        QSignalSpy spy(&emitter, &Emitter::ticked);
        QTimer::singleShot(10, &emitter, [&] { emitter.tick(); });
        QVERIFY(spy.wait(5000));
        QVERIFY(!spy.wait(std::chrono::milliseconds(20)));
        QCOMPARE(spy.count(), 1);
    }

    void waiting() {
        int calls = 0;
        QTimer::singleShot(10, this, [&] { calls = 1; });
        QVERIFY(QTest::qWaitFor([&] { return calls == 1; }));
        QVERIFY(!QTest::qWaitFor([] { return false; }, 20));
        QVERIFY(!QTest::qWaitFor([] { return false; }, std::chrono::milliseconds(20)));

        calls = 0;
        QTimer::singleShot(5, this, [&] { calls = 1; });
        QTest::qWait(100);
        QCOMPARE(calls, 1);
        QTest::qSleep(1);
        QTest::qWait(std::chrono::milliseconds(1));
    }

    void tryMacros() {
        int value = 0;
        QTimer::singleShot(10, this, [&] { value = 2; });
        QTRY_VERIFY(value == 2);
        value = 0;
        QTimer::singleShot(10, this, [&] { value = 2; });
        QTRY_COMPARE(value, 2);
        QTRY_VERIFY2(value == 2, "still");
        QTRY_VERIFY_WITH_TIMEOUT(value == 2, 100);
        QTRY_COMPARE_WITH_TIMEOUT(value, 2, 100);
    }

    void eventLoop() {
        QTestEventLoop &loop = QTestEventLoop::instance();
        QTimer::singleShot(5, &loop, &QTestEventLoop::exitLoop);
        loop.enterLoopMSecs(5000);
        QVERIFY(!loop.timeout());
        loop.enterLoopMSecs(5);
        QVERIFY(loop.timeout());
        loop.enterLoop(std::chrono::milliseconds(5));
        QVERIFY(loop.timeout());
        QTimer::singleShot(1, &loop, [&] { loop.changeInterval(5); });
        loop.enterLoop(5);
        QVERIFY(loop.timeout());
        loop.exitLoop(); // not running: nothing happens
        loop.changeInterval(5);
    }

    void exceptions() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, throw std::runtime_error("x"));
        QVERIFY_THROWS_EXCEPTION(std::exception, throw std::runtime_error("x"));
        QVERIFY_THROWS_NO_EXCEPTION((void)0);
    }

    void messages() {
        QTest::ignoreMessage(QtWarningMsg, "first");
        QTest::ignoreMessage(QtDebugMsg, "second");
        qDebug("second");
        qWarning("first");
    }

    void messagePatterns() {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^retry [0-9]+ of 3$"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("timed out"));
        QTest::ignoreMessage(QtDebugMsg, "exact");
        qWarning("retry 2 of 3");
        qDebug("exact");
        qWarning("the request timed out after 5 s");
    }

    void deferredDelete() {
        // Deleted between rows; ASan reports a leak if it never is.
        (new QObject(nullptr))->deleteLater();
    }

    void toStrings() {
        const auto text = [](char *value) {
            std::unique_ptr<char[]> owned(value);
            return QString::fromUtf8(owned.get());
        };
        QCOMPARE(text(QTest::toString(true)), "true");
        QCOMPARE(text(QTest::toString(-5)), "-5");
        QCOMPARE(text(QTest::toString(5ULL)), "5");
        QCOMPARE(text(QTest::toString(0.5)), "0.5");
        QCOMPARE(text(QTest::toString(0.1F)), "0.1");
        QCOMPARE(text(QTest::toString('a')), "\"a\"");
        QCOMPARE(text(QTest::toString("a\"b\n")), "\"a\\\"b\\n\"");
        QCOMPARE(text(QTest::toString(static_cast<const char *>(nullptr))), "nullptr");
        QCOMPARE(text(QTest::toString(nullptr)), "nullptr");
        QCOMPARE(text(QTest::toString(QString("é"))), "\"é\"");
        QCOMPARE(text(QTest::toString(QStringView(u"v"))), "\"v\"");
        QCOMPARE(text(QTest::toString(QLatin1StringView("l"))), "\"l\"");
        QCOMPARE(text(QTest::toString(QChar(u'c'))), "\"c\"");
        QCOMPARE(text(QTest::toString(QByteArray("\x01"))), "\"\\x01\"");
        QCOMPARE(text(QTest::toString(QStringList{"a", "b"})), "QStringList(\"a\", \"b\")");
        QCOMPARE(text(QTest::toString(QList<int>{1, 2})), "QList(1, 2)");
        QCOMPARE(text(QTest::toString(std::make_pair(1, QString("a")))), "std::pair(1, \"a\")");
        QCOMPARE(text(QTest::toString(std::make_tuple(1, true))), "std::tuple(1, true)");
        QCOMPARE(text(QTest::toString(Color::Green)), "2");
        QCOMPARE(text(QTest::toString(QDate(2026, 11, 7))), "2026-11-07");
        QCOMPARE(text(QTest::toString(QDate())), "Invalid QDate");
        QCOMPARE(text(QTest::toString(QTime(1, 2, 3))), "01:02:03.000");
        QCOMPARE(text(QTest::toString(QTime())), "Invalid QTime");
        QVERIFY(text(QTest::toString(QDateTime(QDate(2026, 11, 7), QTime(1, 2)))).startsWith("2026-11-07T01:02"));
        QCOMPARE(text(QTest::toString(QDateTime())), "Invalid QDateTime");
        QCOMPARE(text(QTest::toString(QUrl("http://a.example/"))), "\"http://a.example/\"");
        QCOMPARE(text(QTest::toString(QVersionNumber(6, 1))), "6.1");
        QCOMPARE(text(QTest::toString(QUuid())), "{00000000-0000-0000-0000-000000000000}");
        QCOMPARE(text(QTest::toString(static_cast<int *>(nullptr))), "nullptr");
        QVERIFY(!text(QTest::toString(this)).isEmpty());
        QVERIFY(!QTest::toString(Opaque()));
    }

    void findTestData() {
        QVERIFY(QFINDTESTDATA("test_qtest.cpp").endsWith("test_qtest.cpp"));
        QVERIFY(QFile::exists(QFINDTESTDATA("test_qtest.cpp")));
        QVERIFY(QFile::exists(QFINDTESTDATA(QFINDTESTDATA("test_qtest.cpp"))));
    }

private:
    QString m_calls;
    int m_inits = 0;
    int m_cleanups = 0;
};

class Failing : public QObject {
    Q_OBJECT

private slots:
    void verifyStops() {
        reached = 1;
        QVERIFY(1 == 2);
        reached = 2;
    }
    void verifyMessage() { QVERIFY2(false, "the reason"); }
    void compareValues() {
        const QString actual = "abc";
        QCOMPARE(actual, QString("abd"));
    }
    void compareUserTypes() { QCOMPARE(geometry::Point({1, 2}), geometry::Point({3, 4})); }
    void compareSpecialized() { QCOMPARE(Specialized(), Specialized()); }
    void compareOpaque() { QCOMPARE(Opaque(), Opaque()); }
    void compareListSize() { QCOMPARE(QList<int>({1}), QList<int>({1, 2})); }
    void compareListItem() { QCOMPARE(QList<int>({1, 2}), QList<int>({1, 3})); }
    void compareOperator() { QCOMPARE_LT(3, 2); }
    void compareFloat() { QCOMPARE(1.0, 1.001); }
    void failNow() { QFAIL("told to fail"); }
    void uncaught() { throw std::runtime_error("boom"); }
    void uncaughtOther() { throw 1; }
    void wrongException() { QVERIFY_THROWS_EXCEPTION(std::runtime_error, throw 1); }
    void noException() { QVERIFY_THROWS_EXCEPTION(std::runtime_error, (void)0); }
    void unexpectedException() { QVERIFY_THROWS_NO_EXCEPTION(throw 1); }
    void timedOut() { QTRY_VERIFY_WITH_TIMEOUT(false, 5); }
    void missingMessage() { QTest::ignoreMessage(QtWarningMsg, "never sent"); }
    void missingPattern() {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^retry [0-9]+$"));
        qWarning("retry x");
    }
    void wrongMessageType() {
        QTest::ignoreMessage(QtWarningMsg, "typed");
        qDebug("typed");
    }
    void rows_data() {
        QTest::addColumn<int>("number");
        QTest::newRow("passes") << 1;
        QTest::newRow("fails") << 2;
    }
    void rows() {
        QFETCH(int, number);
        QCOMPARE(number, 1);
    }

public:
    int reached = 0;
};

class Expected : public QObject {
    Q_OBJECT

private slots:
    void abort() {
        QEXPECT_FAIL("", "known bug", Abort);
        QVERIFY(false);
        after = 1;
    }
    void carryOn() {
        QEXPECT_FAIL("", "known bug", Continue);
        QCOMPARE(1, 2);
        QCOMPARE(after, 0);
        continued = true;
    }
    void unexpectedPass() {
        QEXPECT_FAIL("", "should have failed", Abort);
        QVERIFY(true);
    }
    void neverChecked() { QEXPECT_FAIL("", "dangling", Abort); }
    void twice() {
        QEXPECT_FAIL("", "one", Continue);
        QEXPECT_FAIL("", "two", Continue);
        QVERIFY(false);
    }
    void byTag_data() {
        QTest::addColumn<int>("number");
        QTest::newRow("broken") << 1;
        QTest::newRow("working") << 2;
    }
    void byTag() {
        QFETCH(int, number);
        QEXPECT_FAIL("broken", "row bug", Abort);
        QCOMPARE(number, 2);
    }

public:
    int after = 0;
    bool continued = false;
};

class Skipping : public QObject {
    Q_OBJECT

private slots:
    void all() {
        QSKIP("not here");
        ran = true;
    }
    void oneRow_data() {
        QTest::addColumn<bool>("skip");
        QTest::newRow("skipped") << true;
        QTest::newRow("runs") << false;
    }
    void oneRow() {
        QFETCH(bool, skip);
        if (skip)
            QSKIP("this row");
    }
    void wholeData_data() { QSKIP("no data today"); }
    void wholeData() { ran = true; }
    void emptyTable_data() { QTest::addColumn<int>("number"); }
    void emptyTable() { ran = true; }
    void failingData_data() { QFAIL("bad data"); }
    void failingData() { ran = true; }

public:
    bool ran = false;
};

class FailingInit : public QObject {
    Q_OBJECT

private slots:
    void init() {
        ++inits;
        if (inits == 1)
            QFAIL("no fixture");
    }
    void cleanup() { ++cleanups; }
    void first() { ++ran; }
    void second() { ++ran; }

public:
    int inits = 0;
    int cleanups = 0;
    int ran = 0;
};

class FailingCase : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QFAIL("cannot start"); }
    void cleanupTestCase() { cleanedUp = true; }
    void test() { ran = true; }

public:
    bool ran = false;
    bool cleanedUp = false;
};

class SkippedCase : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QSKIP("not on this system"); }
    void cleanupTestCase() { cleanedUp = true; }
    void test() { ran = true; }

public:
    bool ran = false;
    bool cleanedUp = false;
};

class GlobalData : public QObject {
    Q_OBJECT

private slots:
    void initTestCase_data() {
        QTest::addColumn<QString>("backend");
        QTest::newRow("fast") << "F";
        QTest::newRow("slow") << "S";
    }
    void init() {
        QFETCH_GLOBAL(QString, backend);
        seen += backend;
    }
    void plain() {
        QFETCH(QString, backend); // falls back to the global row
        QCOMPARE(QString(QTest::currentGlobalDataTag()).left(1).toUpper(), backend);
    }
    void rows_data() {
        QTest::addColumn<int>("number");
        QTest::newRow("a") << 1;
        QTest::newRow("b") << 2;
    }
    void rows() {
        QFETCH(int, number);
        QVERIFY(number > 0);
        QVERIFY(QTest::currentDataTag());
    }

public:
    QString seen;
};

class Base : public QObject {
    Q_OBJECT

public:
    QString calls;

protected slots:
    void notATest() { calls += "protected "; }

private slots:
    virtual void init() { calls += "base-init "; }
    void inherited() { calls += "inherited "; }
    void withArgument(int) { calls += "argument "; }

public slots:
    void publicSlot() { calls += "public "; }
};

// Has no private slots, but links Derived to Base.
class Middle : public Base {
    Q_OBJECT
};

class Derived : public Middle {
    Q_OBJECT

private slots:
    void init() override { calls += "derived-init "; }
    void own() { calls += "own "; }
    void declaredAway();
    void inlineWithBody() const noexcept { const_cast<Derived *>(this)->calls += "inline "; }
    int returnsValue() { return 1; }
};

void Derived::declaredAway() { calls += "away "; }

class WithMessages : public QObject {
    Q_OBJECT

private slots:
    void log() {
        qDebug("a debug line");
        qInfo("an info line");
        qWarning("a warning line");
        qCritical("a critical line");
    }
    void escaping() { QFAIL("<&>\"'"); }
};

class NoMoc : public QObject {};

static std::string g_log;
static QTemporaryDir *g_dir = nullptr;

// Runs `object` and leaves the log in g_log. Returns qExec()'s result.
static int run(QObject *object, QStringList arguments = {}, const char *format = "txt") {
    const QString path = g_dir->filePath("log");
    QStringList all{"tst_fixture", "-o", path + "," + format};
    all += arguments;
    const int result = QTest::qExec(object, all);
    QFile file(path);
    g_log.clear();
    if (file.open(QIODevice::ReadOnly))
        g_log = file.readAll().toStdString();
    return result;
}

static bool logged(const std::string &text) {
    if (g_log.find(text) != std::string::npos)
        return true;
    std::printf("not in the log: %s\n", text.c_str());
    return false;
}

static bool absent(const std::string &text) { return g_log.find(text) == std::string::npos; }

static void testGood() {
    Good good;
    CHECK(run(&good) == 0);
    CHECK(good.cleanedUp);
    CHECK(logged("********* Start testing of Good *********"));
    CHECK(logged("Config: Using Nt Test "));
    CHECK(logged("PASS   : Good::initTestCase()"));
    CHECK(logged("PASS   : Good::verify()"));
    CHECK(logged("PASS   : Good::data(first)"));
    CHECK(logged("PASS   : Good::data(row-4)"));
    CHECK(logged("PASS   : Good::cleanupTestCase()"));
    CHECK(logged(" 0 failed, 0 skipped, 0 blacklisted, "));
    CHECK(logged("********* Finished testing of Good *********"));
    CHECK(absent("FAIL!"));
    CHECK(absent("init()"));
    CHECK(absent("data_data"));
}

static void testFailures() {
    Failing failing;
    const int failures = run(&failing);
    CHECK(failures == 21);
    CHECK(failing.reached == 1);
    CHECK(logged("FAIL!  : Failing::verifyStops() '1 == 2' returned FALSE. ()\n   Loc: ["));
    CHECK(logged("test_qtest.cpp("));
    CHECK(logged("'false' returned FALSE. (the reason)"));
    CHECK(logged("Compared values are not the same\n"
                 "   Actual   (actual)        : \"abc\"\n"
                 "   Expected (QString(\"abd\")): \"abd\"\n"));
    CHECK(logged(": 1,2\n"));
    CHECK(logged(": 3,4\n"));
    CHECK(logged(": specialized\n"));
    CHECK(logged(": (no string representation)\n"));
    CHECK(logged("Compared lists have different sizes: 1 and 2"));
    CHECK(logged("Compared lists differ at index 1"));
    CHECK(logged("The computed value is expected to be less than the baseline, but is not\n"
                 "   Computed (3): 3\n   Baseline (2): 2\n"));
    CHECK(logged(": 1.001\n"));
    CHECK(logged("FAIL!  : Failing::failNow() told to fail"));
    CHECK(logged("Caught unhandled exception: boom"));
    CHECK(logged("FAIL!  : Failing::uncaughtOther() Caught unhandled exception\n"));
    CHECK(logged("Expected exception of type std::runtime_error, but caught another type"));
    CHECK(logged("Expected exception of type std::runtime_error to be thrown, but none was"));
    CHECK(logged("Expected no exception to be thrown, but caught one"));
    CHECK(logged("FAIL!  : Failing::timedOut() 'false' returned FALSE."));
    CHECK(logged("FAIL!  : Failing::missingMessage() Did not receive the message: \"never sent\""));
    CHECK(logged("QDEBUG : Failing::wrongMessageType() typed"));
    CHECK(logged("FAIL!  : Failing::missingPattern() Did not receive the message: \"^retry [0-9]+$\""));
    CHECK(logged("QWARN  : Failing::missingPattern() retry x"));
    CHECK(logged("PASS   : Failing::rows(passes)"));
    CHECK(logged("FAIL!  : Failing::rows(fails) Compared values are not the same"));
    CHECK(logged("Totals: 3 passed, 21 failed, 0 skipped"));
}

static void testExpectedFailures() {
    Expected expected;
    CHECK(run(&expected) == 3);
    CHECK(expected.after == 0);
    CHECK(expected.continued);
    CHECK(logged("XFAIL  : Expected::abort() known bug\n   Loc: ["));
    CHECK(logged("PASS   : Expected::abort()"));
    CHECK(logged("XFAIL  : Expected::carryOn() known bug"));
    CHECK(logged("XPASS  : Expected::unexpectedPass() Passed unexpectedly: should have failed"));
    CHECK(logged("FAIL!  : Expected::neverChecked() QEXPECT_FAIL was called without any check after it"));
    CHECK(logged("FAIL!  : Expected::twice() QEXPECT_FAIL was called twice"));
    CHECK(logged("XFAIL  : Expected::byTag(broken) row bug"));
    CHECK(logged("PASS   : Expected::byTag(working)"));
}

static void testSkips() {
    Skipping skipping;
    CHECK(run(&skipping) == 1);
    CHECK(!skipping.ran);
    CHECK(logged("SKIP   : Skipping::all() not here\n   Loc: ["));
    CHECK(logged("SKIP   : Skipping::oneRow(skipped) this row"));
    CHECK(logged("PASS   : Skipping::oneRow(runs)"));
    CHECK(logged("SKIP   : Skipping::wholeData() no data today"));
    CHECK(logged("SKIP   : Skipping::emptyTable() No data available"));
    CHECK(logged("FAIL!  : Skipping::failingData() bad data"));
    CHECK(logged("Totals: 3 passed, 1 failed, 4 skipped"));
}

static void testLifecycleFailures() {
    FailingInit failingInit;
    CHECK(run(&failingInit) == 1);
    CHECK(failingInit.ran == 1);
    CHECK(failingInit.inits == 2 && failingInit.cleanups == 2);
    CHECK(logged("FAIL!  : FailingInit::first() no fixture"));

    FailingCase failingCase;
    CHECK(run(&failingCase) == 1);
    CHECK(!failingCase.ran);
    CHECK(failingCase.cleanedUp);
    CHECK(logged("FAIL!  : FailingCase::initTestCase() cannot start"));

    SkippedCase skippedCase;
    CHECK(run(&skippedCase) == 0);
    CHECK(!skippedCase.ran);
    CHECK(!skippedCase.cleanedUp);
    CHECK(logged("Totals: 0 passed, 0 failed, 1 skipped"));
}

static void testGlobalData() {
    GlobalData data;
    CHECK(run(&data) == 0);
    CHECK(data.seen == "FFFSSS");
    CHECK(logged("PASS   : GlobalData::plain(fast)"));
    CHECK(logged("PASS   : GlobalData::plain(slow)"));
    CHECK(logged("PASS   : GlobalData::rows(fast:a)"));
    CHECK(logged("PASS   : GlobalData::rows(slow:b)"));

    data.seen.clear();
    CHECK(run(&data, {"rows:slow:a"}) == 0);
    CHECK(data.seen == "S");
    CHECK(run(&data, {"rows:fast"}) == 0);
    CHECK(logged("PASS   : GlobalData::rows(fast:b)"));
    CHECK(absent("(slow"));
}

static void testInheritance() {
    Derived derived;
    CHECK(run(&derived) == 0);
    CHECK(derived.calls == "derived-init inherited derived-init own derived-init away derived-init inline ");
    CHECK(logged("Start testing of Derived"));
}

static void testSelection() {
    Good good;
    CHECK(run(&good, {"verify", "data:literals"}) == 0);
    CHECK(logged("PASS   : Good::verify()"));
    CHECK(logged("PASS   : Good::data(literals)"));
    CHECK(absent("Good::data(first)"));
    CHECK(absent("Good::spy()"));

    Good unknownTag;
    CHECK(run(&unknownTag, {"data:nothing"}) == 1);
    CHECK(logged("Unknown data tag: 'nothing'"));

    Good other;
    CHECK(QTest::qExec(&other, QStringList{"tst", "nothing"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "init"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-nonsense"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-o"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-o", "x,csv"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-o", "/nonexistent/dir/log,txt"}) == 1);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-functions"}) == 0);
    CHECK(QTest::qExec(&other, QStringList{"tst", "-help"}) == 0);
    CHECK(QString(QTest::currentAppName()) == "tst");
    CHECK(!other.cleanedUp);
}

static void testVerbosity() {
    WithMessages messages;
    CHECK(run(&messages) == 1);
    CHECK(logged("QDEBUG : WithMessages::log() a debug line"));
    CHECK(logged("QINFO  : WithMessages::log() an info line"));
    CHECK(logged("QWARN  : WithMessages::log() a warning line"));
    CHECK(logged("QCRITICAL: WithMessages::log() a critical line"));
    CHECK(absent("entering"));

    CHECK(run(&messages, {"-v2"}) == 1);
    CHECK(logged("INFO   : WithMessages::log() entering"));

    CHECK(run(&messages, {"-silent"}) == 1);
    CHECK(absent("PASS"));
    CHECK(absent("a debug line"));
    CHECK(logged("FAIL!  : WithMessages::escaping()"));
    CHECK(logged("Totals:"));
}

static void testFormats() {
    WithMessages messages;
    CHECK(run(&messages, {}, "junitxml") == 1);
    CHECK(logged("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"));
    CHECK(logged("<testsuite name=\"WithMessages\" tests=\"4\" failures=\"1\" errors=\"0\" skipped=\"0\""));
    CHECK(logged("<testcase name=\"WithMessages::log()\" time=\""));
    CHECK(logged("<failure type=\"fail\" message=\"&lt;&amp;&gt;&quot;&apos;\">"));
    CHECK(logged("<system-err>QDEBUG : a debug line"));
    CHECK(logged("</testsuite>"));

    Skipping skipping;
    CHECK(run(&skipping, {"all", "failingData"}, "junitxml") == 1);
    CHECK(logged("<skipped message=\"not here\"/>"));

    CHECK(run(&messages, {}, "tap") == 1);
    CHECK(logged("TAP version 13\n# WithMessages\n"));
    CHECK(logged("ok 1 - WithMessages::initTestCase()"));
    CHECK(logged("# QDEBUG : a debug line"));
    CHECK(logged("not ok 3 - WithMessages::escaping()\n  ---\n  message: |\n    <&>\"'\n  at: "));
    CHECK(logged("1..4\n# tests 4\n# pass 3\n# fail 1\n"));

    CHECK(run(&skipping, {"all"}, "tap") == 0);
    CHECK(logged("ok 2 - Skipping::all() # SKIP not here"));

    // The format given by a flag applies to an -o without one.
    const QString path = g_dir->filePath("flag");
    CHECK(QTest::qExec(&skipping, QStringList{"tst", "-tap", "-o", path, "all"}) == 0);
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly) && file.readAll().startsWith("TAP version 13"));
}

static void testErrors() {
    NoMoc noMoc;
    CHECK(QTest::qExec(&noMoc) == 1);
    CHECK(QTest::qExec(nullptr) == 1);
}

// The return value is the number of failures, capped to what a process can report.
class ManyFailures : public QObject {
    Q_OBJECT

private slots:
    void rows_data() {
        QTest::addColumn<int>("number");
        for (int i = 0; i < 200; ++i)
            QTest::addRow("%d", i) << i;
    }
    void rows() { QFAIL("every row"); }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    g_dir = &dir;

    testGood();
    testFailures();
    testExpectedFailures();
    testSkips();
    testLifecycleFailures();
    testGlobalData();
    testInheritance();
    testSelection();
    testVerbosity();
    testFormats();
    testErrors();

    ManyFailures many;
    CHECK(run(&many) == 127);
    CHECK(logged("Totals: 2 passed, 200 failed"));

    // No state may be left behind by the runs above.
    CHECK(!QTest::currentTestFunction());
    CHECK(!QTest::currentDataTag());
    CHECK(!QTest::currentTestFailed());

    if (g_failures == 0)
        std::printf("all test library tests passed\n");
    return g_failures == 0 ? 0 : 1;
}

#include "test_qtest.moc"
