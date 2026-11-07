// MIT License - see LICENSE
#include <QtCore>
#include <QDebug>
#include <QSet>
#include <QSocketNotifier>

#include <unistd.h>

#include <cstdio>
#include <thread>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static void testString() {
    QString s("héllo wörld");
    CHECK(s.size() == 11);
    CHECK(s.toUtf8().size() == 13);
    CHECK(QString::fromUtf8(s.toUtf8()) == s);
    CHECK(s.toUpper() == QString("HÉLLO WÖRLD"));
    CHECK(QString("  a  b  ").simplified() == "a b");
    CHECK(QString("  a  ").trimmed() == "a");
    CHECK(QString("a,b,,c").split(',').size() == 4);
    CHECK(QString("a,b,,c").split(',', Qt::SkipEmptyParts).size() == 3);
    CHECK(QString("a,b,c").split(',').join("-") == "a-b-c");
    CHECK(QString("%1 + %2").arg(1).arg(2) == "1 + 2");
    CHECK(QString("%1").arg(3.5) == "3.5");
    CHECK(QString("%1").arg(7, 3, 10, QChar('0')) == "007");
    CHECK(QString("%2 %1").arg("a", "b") == "b a");
    CHECK(QString("x").arg(1) == "x 1");
    CHECK(QString::number(255, 16) == "ff");
    CHECK(QString::number(1.5, 'f', 2) == "1.50");
    bool ok = false;
    CHECK(QString(" 42 ").toInt(&ok) == 42 && ok);
    QString(" 4 2").toInt(&ok);
    CHECK(!ok);
    CHECK(QString("0x1f").toInt(&ok, 16) == 31 && ok);
    CHECK(QString("1e3").toDouble(&ok) == 1000.0 && ok);
    CHECK(QString("abc").toInt(&ok) == 0 && !ok);
    CHECK(QString("hello").mid(1, 3) == "ell");
    CHECK(QString("hello").left(2) == "he" && QString("hello").right(2) == "lo");
    CHECK(QString("Hello").startsWith("he", Qt::CaseInsensitive));
    CHECK(QString("Hello").indexOf("LL", 0, Qt::CaseInsensitive) == 2);
    CHECK(QString("aXbXc").replace("X", "--") == "a--b--c");
    CHECK(QString("abc").compare("ABC", Qt::CaseInsensitive) == 0);
    CHECK(QString().isNull() && QString("").isEmpty() && !QString("").isNull());
    CHECK((QString("a") + 'b' + QChar('c') + "d") == "abcd");
    CHECK(QStringLiteral("lit").size() == 3);
    CHECK(QString("𝄞").size() == 2);
    CHECK(QString("𝄞").toUtf8().size() == 4);
    CHECK(QString("x").repeated(3) == "xxx");
    CHECK(QString("7").rightJustified(3, '0') == "007");
    CHECK(QString("a/b/c").section('/', 1, 1) == "b");
    QStringList l;
    l << "b" << "a" << "b";
    l.removeDuplicates();
    l.sort();
    CHECK(l.join(",") == "a,b");
    using namespace Qt::Literals::StringLiterals;
    CHECK(u"abc"_s == "abc");
}

static void testByteArray() {
    QByteArray b("hello");
    CHECK(b.size() == 5 && b.toUpper() == "HELLO");
    CHECK(b.toHex() == "68656c6c6f");
    CHECK(QByteArray::fromHex("68656c6c6f") == b);
    CHECK(b.toBase64() == "aGVsbG8=");
    CHECK(QByteArray::fromBase64("aGVsbG8=") == b);
    CHECK(b.toBase64(QByteArray::OmitTrailingEquals) == "aGVsbG8");
    CHECK(QByteArray("a,b").split(',').size() == 2);
    CHECK(QByteArray::number(42) == "42" && QByteArray("42").toInt() == 42);
    CHECK(QByteArray("a b").toPercentEncoding() == "a%20b");
    CHECK(QByteArray::fromPercentEncoding("a%20b") == "a b");
    CHECK(b.indexOf("ll") == 2 && b.contains('e') && b.startsWith("he") && b.endsWith("lo"));
    b.replace("l", "L");
    CHECK(b == "heLLo");
    CHECK(QByteArray().isNull() && !QByteArray("").isNull());
}

static void testContainers() {
    QList<int> l{3, 1, 2};
    l.append(5);
    l.prepend(0);
    CHECK(l.size() == 5 && l.first() == 0 && l.last() == 5 && l.indexOf(2) == 3);
    std::sort(l.begin(), l.end());
    CHECK((l == QList<int>{0, 1, 2, 3, 5}));
    CHECK(l.takeFirst() == 0 && l.mid(1, 2) == (QList<int>{2, 3}));
    l.removeAll(2);
    CHECK(!l.contains(2) && l.value(99, -1) == -1);

    QMap<QString, int> m;
    m["b"] = 2;
    m.insert("a", 1);
    m.insert("a", 10);
    CHECK(m.size() == 2 && m.value("a") == 10 && m.value("zz", 7) == 7);
    CHECK(m.keys() == (QList<QString>{"a", "b"}));
    int sum = 0;
    for (int v : m)
        sum += v;
    CHECK(sum == 12);
    for (auto it = m.begin(); it != m.end(); ++it)
        it.value() += 1;
    CHECK(m["a"] == 11 && m.firstKey() == "a" && m.take("b") == 3 && !m.contains("b"));

    QMultiMap<int, QString> mm;
    mm.insert(1, "x");
    mm.insert(1, "y");
    CHECK(mm.size() == 2 && mm.values(1).size() == 2 && mm.count(1) == 2);

    QHash<QString, int> h;
    h["one"] = 1;
    h.insert("two", 2);
    CHECK(h.value("two") == 2 && h.contains("one") && h.remove("one") == 1 && h.size() == 1);
    QHash<int, QString> h2;
    h2[5] = "five";
    CHECK(h2.value(5) == "five");

    QSet<QString> s;
    s << "a" << "b" << "a";
    CHECK(s.size() == 2 && s.contains("a"));
    QSet<int> a{1, 2, 3}, b{2, 3, 4};
    CHECK((a & b).size() == 2 && (a | b).size() == 4 && (a - b).size() == 1);

    QQueue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    CHECK(q.dequeue() == 1 && q.head() == 2);
    QStack<int> st;
    st.push(1);
    st.push(2);
    CHECK(st.pop() == 2 && st.top() == 1);
}

class Tracked : public QObject {
public:
    explicit Tracked(QObject *parent = nullptr) : QObject(parent) {}
};

struct Payload : QSharedData {
    int v = 0;
};

static void testPointers() {
    QPointer<Tracked> p;
    {
        auto *t = new Tracked;
        p = t;
        CHECK(!p.isNull() && p.data() == t);
        delete t;
    }
    CHECK(p.isNull() && p == nullptr);

    QScopedPointer<int> sp(new int(5));
    CHECK(*sp == 5 && !sp.isNull());
    int *raw = sp.take();
    CHECK(sp.isNull());
    delete raw;

    auto shared = QSharedPointer<int>::create(7);
    QWeakPointer<int> weak = shared;
    CHECK(*shared == 7 && !weak.isNull() && weak.toStrongRef().data() == shared.data());
    shared.clear();
    CHECK(weak.isNull());

    QSharedDataPointer<Payload> a(new Payload), b;
    b = a;
    a->v = 3;
    CHECK(a->v == 3 && b->v == 0);

    Tracked parent;
    auto *c = new Tracked(&parent);
    c->setObjectName("kid");
    CHECK(parent.findChild<Tracked *>("kid") == c && parent.findChildren<Tracked *>().size() == 1);
    CHECK(parent.findChild<Tracked *>("nope") == nullptr);

    bool ran = false;
    {
        auto guard = qScopeGuard([&] { ran = true; });
    }
    CHECK(ran);
    QSignalBlocker blocker(&parent);
    CHECK(parent.signalsBlocked());
}

static void testSync() {
    QMutex mutex;
    int counter = 0;
    std::thread t1([&] { for (int i = 0; i < 1000; ++i) { QMutexLocker<QMutex> l(&mutex); ++counter; } });
    std::thread t2([&] { for (int i = 0; i < 1000; ++i) { QMutexLocker<QMutex> l(&mutex); ++counter; } });
    t1.join();
    t2.join();
    CHECK(counter == 2000);

    QSemaphore sem;
    std::thread t3([&] { sem.release(2); });
    sem.acquire(2);
    t3.join();
    CHECK(sem.available() == 0 && !sem.tryAcquire(1, 5));

    QWaitCondition cond;
    bool ready = false;
    std::thread t4([&] { QMutexLocker<QMutex> l(&mutex); ready = true; cond.wakeAll(); });
    {
        QMutexLocker<QMutex> l(&mutex);
        while (!ready)
            cond.wait(&mutex);
    }
    t4.join();
    CHECK(ready);

    QReadWriteLock rw;
    rw.lockForRead();
    CHECK(rw.tryLockForRead() && !rw.tryLockForWrite(5));
    rw.unlock();
    rw.unlock();
    rw.lockForWrite();
    rw.unlock();

    QAtomicInt ai(1);
    CHECK(ai.ref() && ++ai == 3);

    QElapsedTimer et;
    et.start();
    QDeadlineTimer dl(20);
    CHECK(!dl.hasExpired() && dl.remainingTime() > 0);
    while (!dl.hasExpired()) {
    }
    CHECK(et.elapsed() >= 19 && dl.remainingTime() == 0 && QDeadlineTimer(QDeadlineTimer::Forever).isForever());
}

static QList<QString> g_messages;
static void captureHandler(QtMsgType, const QMessageLogContext &, const QString &msg) { g_messages.append(msg); }

static void testDebug() {
    qInstallMessageHandler(captureHandler);
    qDebug() << "a" << 1 << QString("q") << QList<int>{1, 2} << true;
    qWarning("printf %d", 7);
    qInstallMessageHandler(nullptr);
    CHECK(g_messages.size() == 2);
    CHECK(g_messages.value(0) == "a 1 \"q\" (1, 2) true");
    CHECK(g_messages.value(1) == "printf 7");
}

static void testIO() {
    QBuffer buf;
    CHECK(buf.open(QIODevice::ReadWrite));
    buf.write("line1\nline2\nrest");
    buf.seek(0);
    CHECK(buf.canReadLine());
    CHECK(buf.readLine() == "line1\n");
    CHECK(buf.peek(3) == "lin" && buf.pos() == 6);
    CHECK(buf.readLine() == "line2\n");
    CHECK(buf.readAll() == "rest" && buf.atEnd());
    char c;
    buf.seek(2);
    CHECK(buf.getChar(&c) && c == 'n');
    buf.ungetChar('n');
    CHECK(buf.pos() == 2);

    const QString path = QString::fromStdString("/tmp/nt_core_test_" + std::to_string(getpid()) + ".txt");
    {
        QFile f(path);
        CHECK(!f.exists());
        CHECK(f.open(QIODevice::WriteOnly));
        CHECK(f.write("alpha\nbeta\ngamma\n") == 17);
        f.close();
    }
    {
        QFile f(path);
        CHECK(f.exists() && f.size() == 17);
        CHECK(f.open(QIODevice::ReadOnly));
        CHECK(f.readLine() == "alpha\n");
        CHECK(f.pos() == 6);
        CHECK(f.readLine() == "beta\n" && f.readAll() == "gamma\n" && f.atEnd());
        CHECK(!f.open(QIODevice::ReadOnly));
        f.close();
        CHECK(!f.open(QIODevice::ReadOnly | QIODevice::WriteOnly | QIODevice::NewOnly));
        CHECK(f.open(QIODevice::Append));
        f.write("delta\n");
        f.close();
        CHECK(f.size() == 23);
    }
    CHECK(QFile::copy(path, path + ".copy") && QFile::exists(path + ".copy"));
    CHECK(QFile::remove(path) && QFile::remove(path + ".copy") && !QFile::exists(path));
    QFile missing("/nonexistent/dir/file");
    CHECK(!missing.open(QIODevice::ReadOnly) && missing.error() == QFileDevice::OpenError);
}

static void testSocketNotifier() {
    int fds[2];
    CHECK(pipe(fds) == 0);
    QSocketNotifier n(fds[0], QSocketNotifier::Read);
    int hits = 0;
    QEventLoop loop;
    QObject::connect(&n, &QSocketNotifier::activated, [&](QSocketDescriptor sd, QSocketNotifier::Type t) {
        char b[8];
        CHECK(sd == fds[0] && t == QSocketNotifier::Read);
        CHECK(read(fds[0], b, sizeof b) == 1);
        ++hits;
        loop.quit();
    });
    QTimer::singleShot(5, [&] { CHECK(write(fds[1], "x", 1) == 1); });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(hits == 1);

    n.setEnabled(false);
    CHECK(write(fds[1], "y", 1) == 1);
    QCoreApplication::processEvents();
    CHECK(hits == 1);
    close(fds[0]);
    close(fds[1]);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testString();
    testByteArray();
    testContainers();
    testPointers();
    testSync();
    testDebug();
    testIO();
    testSocketNotifier();
    if (g_failures == 0)
        std::puts("all core tests passed");
    return g_failures == 0 ? 0 : 1;
}
