// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qatomicscopedvaluerollback.h>
#include <QtCore/qcompare.h>
#include <QtCore/qendian.h>
#include <QtCore/qexception.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qglobalstatic.h>
#include <QtCore/qobjectcleanuphandler.h>
#include <QtCore/qoperatingsystemversion.h>
#include <QtCore/qsignalmapper.h>
#include <QtCore/qsysinfo.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testPartialOrdering() {
    const QPartialOrdering less = QPartialOrdering::Less, eq = QPartialOrdering::Equivalent,
                           greater = QPartialOrdering::Greater, un = QPartialOrdering::Unordered;
    CHECK(less < 0 && less <= 0 && !(less > 0) && !(less >= 0) && less != 0 && !(less == 0));
    CHECK(!(eq < 0) && eq <= 0 && !(eq > 0) && eq >= 0 && eq == 0 && !(eq != 0));
    CHECK(!(greater < 0) && !(greater <= 0) && greater > 0 && greater >= 0 && greater != 0);
    CHECK(!(un < 0) && !(un <= 0) && !(un > 0) && !(un >= 0) && !(un == 0) && un != 0);
    CHECK(0 > less && 0 >= less && 0 < greater && 0 <= greater && !(0 < un) && !(0 > un));
    CHECK(less == QPartialOrdering::Less && less != greater && un == QPartialOrdering::Unordered);
    static_assert(QPartialOrdering::Less < 0 && QPartialOrdering::Equivalent == 0, "constexpr");
}

static void testFlag() {
    enum Option { A = 1, B = 2 };
    QFlag f(A | B);
    int i = f;
    CHECK(i == 3 && int(QFlag(5u)) == 5 && int(QFlag(short(-2))) == -2 && int(QFlag(ushort(7))) == 7);
}

static void testEndian() {
    CHECK(qbswap<quint16>(0x1234) == 0x3412);
    CHECK(qbswap<quint32>(0x01020304u) == 0x04030201u);
    CHECK(qbswap<quint64>(0x0102030405060708ULL) == 0x0807060504030201ULL);
    CHECK(qbswap<qint16>(static_cast<qint16>(0x00ff)) == static_cast<qint16>(0xff00));
    CHECK(qbswap<quint8>(0xab) == 0xab);
    CHECK(qbswap(qbswap(3.25f)) == 3.25f && qbswap(qbswap(-1.5e100)) == -1.5e100);

    uchar buf[8];
    qToBigEndian<quint32>(0x01020304u, buf);
    CHECK(buf[0] == 1 && buf[3] == 4);
    qToLittleEndian<quint32>(0x01020304u, buf);
    CHECK(buf[0] == 4 && buf[3] == 1);
    CHECK(qFromBigEndian<quint16>(buf + 1) == 0x0302);
    CHECK(qFromLittleEndian<quint16>(buf + 1) == 0x0203);
    CHECK(qToBigEndian(qFromBigEndian(quint32(0xdeadbeef))) == 0xdeadbeefu);
    constexpr quint16 be = qToBigEndian<quint16>(0x0102);
    CHECK(QSysInfo::ByteOrder == QSysInfo::LittleEndian ? be == 0x0201 : be == 0x0102);

    const uchar src[6] = {0, 1, 0, 2, 0, 3};
    quint16 out[3];
    qFromBigEndian<quint16>(src, 3, out);
    CHECK(out[0] == 1 && out[1] == 2 && out[2] == 3);
    uchar back[6];
    qToBigEndian<quint16>(out, 3, back);
    CHECK(std::memcmp(src, back, 6) == 0);

    quint32_be b = 0x11223344u;
    uchar raw[4];
    std::memcpy(raw, &b, 4);
    CHECK(sizeof(b) == 4 && raw[0] == 0x11 && raw[3] == 0x44);
    quint32 host = b;
    CHECK(host == 0x11223344u);
    b += 1;
    ++b;
    CHECK(quint32(b) == 0x11223346u && b++ == 0x11223346u && b == 0x11223347u);
    qint16_le l = -2;
    std::memcpy(raw, &l, 2);
    CHECK(raw[0] == 0xfe && raw[1] == 0xff && l == -2);
    l *= 3;
    l >>= 1;
    CHECK(l == -3);
    CHECK(quint16_be::max() == 0xffff && qint16_le::min() == -32768);
    struct Header {
        quint16_be length;
        quint32_le id;
    };
    CHECK(sizeof(Header) == 8);
}

static std::atomic<int> g_made{0};
struct Counted {
    Counted() { ++g_made; }
    int value = 7;
};
Q_GLOBAL_STATIC(Counted, counted)
Q_GLOBAL_STATIC_WITH_ARGS(QString, withArgs, (3, QChar('x')))
Q_GLOBAL_STATIC(QList<int>, numbers)

static void testGlobalStatic() {
    CHECK(!counted.exists() && !counted.isDestroyed() && g_made == 0);
    std::vector<std::thread> threads;
    std::vector<Counted *> seen(8);
    for (std::size_t i = 0; i < seen.size(); ++i)
        threads.emplace_back([&, i] { seen[i] = counted(); });
    for (auto &t : threads)
        t.join();
    CHECK(g_made == 1 && counted.exists());
    for (Counted *p : seen)
        CHECK(p == seen[0] && p != nullptr);
    CHECK(counted->value == 7 && (*counted).value == 7);
    Counted *plain = counted;
    CHECK(plain == seen[0]);
    CHECK(*withArgs == "xxx");
    numbers->append(5);
    CHECK(numbers->size() == 1 && (*numbers)[0] == 5);
}

static void testAtomicRollback() {
    std::atomic<int> v{1};
    {
        QAtomicScopedValueRollback<int> r(v);
        v = 5;
        CHECK(v == 5);
    }
    CHECK(v == 1);
    {
        QAtomicScopedValueRollback<int> r(v, 9);
        CHECK(v == 9);
    }
    CHECK(v == 1);
    {
        QAtomicScopedValueRollback<int> r(v, 4, std::memory_order_acq_rel);
        r.commit();
    }
    CHECK(v == 4);
}

static void testMutableSetIterator() {
    QSet<int> set{1, 2, 3, 4, 5, 6};
    QMutableSetIterator<int> it(set);
    int visited = 0;
    while (it.hasNext()) {
        ++visited;
        if (it.next() % 2 == 0) {
            CHECK(it.value() % 2 == 0);
            it.remove();
        }
    }
    CHECK(visited == 6 && set.size() == 3 && set.contains(1) && set.contains(3) && set.contains(5));
    it.toFront();
    it.remove(); // nothing has been returned since the front
    CHECK(set.size() == 3);
    CHECK(it.peekNext() > 0 && it.findNext(3) && it.value() == 3);
    it.remove();
    CHECK(!set.contains(3) && set.size() == 2);
}

static void testRandom64() {
    QRandomGenerator64 a(42), b(42), c(43);
    for (int i = 0; i < 5; ++i)
        CHECK(a.generate() == b.generate());
    CHECK(a() != c());
    CHECK(QRandomGenerator64::min() == 0 && QRandomGenerator64::max() == ~0ULL);
    bool high = false;
    for (int i = 0; i < 64; ++i)
        high |= (a.generate() >> 40) != 0;
    CHECK(high);
    QRandomGenerator64 *sys = QRandomGenerator64::system();
    QRandomGenerator64 *glob = QRandomGenerator64::global();
    CHECK(sys && glob && sys != glob);
    CHECK(sys->generate() != sys->generate() || sys->generate() != sys->generate());
    CHECK(glob->generate() != glob->generate() || glob->generate() != glob->generate());
    QRandomGenerator64 seeded = QRandomGenerator64::securelySeeded();
    CHECK(seeded.bounded(10) < 10);
    QRandomGenerator64 d(7), e(7);
    d.discard(3);
    for (int i = 0; i < 3; ++i)
        e.generate();
    CHECK(d.generate() == e.generate());
    static_assert(std::is_same_v<QRandomGenerator64::result_type, quint64>, "64-bit results");
}

static quint16 bitsOf(qfloat16 h) {
    quint16 bits;
    std::memcpy(&bits, &h, sizeof bits);
    return bits;
}

static void testFloat16() {
    CHECK(sizeof(qfloat16) == 2);
    CHECK(bitsOf(qfloat16(1.0f)) == 0x3c00 && bitsOf(qfloat16(-2.0f)) == 0xc000 && bitsOf(qfloat16(0.0f)) == 0);
    CHECK(bitsOf(qfloat16(65504.0f)) == 0x7bff && bitsOf(qfloat16(65520.0f)) == 0x7c00);
    CHECK(bitsOf(qfloat16(1e10f)) == 0x7c00 && bitsOf(qfloat16(-1e10f)) == 0xfc00);
    CHECK(bitsOf(qfloat16(5.960464477539063e-8f)) == 0x0001 && bitsOf(qfloat16(1e-9f)) == 0);
    CHECK(bitsOf(qfloat16(6.103515625e-5f)) == 0x0400);
    // Ties go to the even mantissa.
    CHECK(bitsOf(qfloat16(1.0f + 1.0f / 2048)) == 0x3c00);
    CHECK(bitsOf(qfloat16(1.0f + 3.0f / 2048)) == 0x3c02);
    CHECK(bitsOf(qfloat16(1.0f + 1.0f / 2048 + 1.0f / 8192)) == 0x3c01);
    CHECK(bitsOf(qfloat16(-0.0f)) == 0x8000 && std::signbit(float(qfloat16(-0.0f))));
    CHECK(bitsOf(qfloat16(3)) == bitsOf(qfloat16(3.0)) && float(qfloat16(7)) == 7.0f);

    // Every half converts to a float and back to the same half.
    int mismatches = 0;
    for (quint32 i = 0; i < 0x10000; ++i) {
        const qfloat16 h = qfloat16FromBits(static_cast<quint16>(i));
        if (h.isNaN()) {
            CHECK(qfloat16(float(h)).isNaN());
            continue;
        }
        if (bitsOf(qfloat16(float(h))) != i)
            ++mismatches;
    }
    CHECK(mismatches == 0);

    qfloat16 h(1.5f);
    CHECK(float(h * 2.0f) == 3.0f && h + h == qfloat16(3.0f) && h - qfloat16(0.5f) == 1.0f && h / 2 == 0.75f);
    h += qfloat16(1.0f);
    h *= qfloat16(2.0f);
    h -= qfloat16(1.0f);
    h /= qfloat16(4.0f);
    CHECK(float(h) == 1.0f && -h == qfloat16(-1.0f));
    CHECK(qfloat16(1.0f) < qfloat16(2.0f) && qfloat16(2.0f) >= 2.0 && 1 < qfloat16(2) && qfloat16(2) != 3.0f);

    const qfloat16 inf = std::numeric_limits<qfloat16>::infinity(), nan = std::numeric_limits<qfloat16>::quiet_NaN();
    CHECK(inf.isInf() && !inf.isFinite() && !inf.isNaN() && qIsInf(inf) && !qIsFinite(inf));
    CHECK(nan.isNaN() && !nan.isFinite() && qIsNaN(nan) && !(nan == nan) && nan != nan);
    CHECK(qfloat16(1.0f).isNormal() && !qfloat16(0.0f).isNormal() && !qfloat16FromBits(1).isNormal());
    CHECK(qfloat16(1.0f).fpClassify() == FP_NORMAL && qfloat16(0.0f).fpClassify() == FP_ZERO &&
          qfloat16FromBits(1).fpClassify() == FP_SUBNORMAL && inf.fpClassify() == FP_INFINITE &&
          nan.fpClassify() == FP_NAN);
    CHECK(float(qfloat16(2.0f).copySign(qfloat16(-1.0f))) == -2.0f);
    CHECK(float(std::numeric_limits<qfloat16>::max()) == 65504.0f &&
          float(std::numeric_limits<qfloat16>::min()) == 6.103515625e-5f);
    CHECK(float(std::numeric_limits<qfloat16>::epsilon()) == 0.0009765625f &&
          float(std::numeric_limits<qfloat16>::lowest()) == -65504.0f &&
          float(std::numeric_limits<qfloat16>::denorm_min()) == 5.960464477539063e-8f);
    CHECK(qFuzzyCompare(qfloat16(1.0f), qfloat16(1.0f)) && !qFuzzyCompare(qfloat16(1.0f), qfloat16(1.5f)));
    CHECK(qFuzzyIsNull(qfloat16(0.0f)) && !qFuzzyIsNull(qfloat16(1.0f)));

    const float in[3] = {1.0f, 0.5f, -3.0f};
    qfloat16 halves[3];
    float out[3];
    qFloatToFloat16(halves, in, 3);
    qFloatFromFloat16(out, halves, 3);
    CHECK(out[0] == 1.0f && out[1] == 0.5f && out[2] == -3.0f);
}

namespace {
class MyException : public QException {
public:
    explicit MyException(int c = 0) : code(c) {}
    void raise() const override { throw *this; }
    MyException *clone() const override { return new MyException(*this); }
    int code;
};
} // namespace

static void testException() {
    try {
        MyException(5).raise();
        CHECK(false);
    } catch (const MyException &e) {
        CHECK(e.code == 5);
    }
    const MyException original(8);
    std::unique_ptr<QException> copy(original.clone());
    CHECK(dynamic_cast<MyException *>(copy.get()) && static_cast<MyException *>(copy.get())->code == 8);
    try {
        QException().raise();
        CHECK(false);
    } catch (const QException &) {
    }
    CHECK(QException().what() != nullptr);

    std::exception_ptr ep;
    try {
        throw std::runtime_error("boom");
    } catch (...) {
        ep = std::current_exception();
    }
    QUnhandledException unhandled(ep);
    try {
        unhandled.raise();
        CHECK(false);
    } catch (const QUnhandledException &e) {
        try {
            std::rethrow_exception(e.exception());
        } catch (const std::runtime_error &r) {
            CHECK(std::string(r.what()) == "boom");
        }
    }
    std::unique_ptr<QUnhandledException> cloned(unhandled.clone());
    CHECK(cloned->exception() == unhandled.exception());
    QUnhandledException fromCatch = [] {
        try {
            throw 42;
        } catch (...) {
            return QUnhandledException();
        }
        return QUnhandledException(nullptr);
    }();
    CHECK(fromCatch.exception() != nullptr);
}

namespace {
std::vector<int> g_deleted;
class Tracked : public QObject {
public:
    explicit Tracked(int i) : id(i) {}
    ~Tracked() override { g_deleted.push_back(id); }
    int id;
};
} // namespace

static void testCleanupHandler() {
    g_deleted.clear();
    {
        QObjectCleanupHandler handler;
        CHECK(handler.isEmpty() && handler.add(nullptr) == nullptr);
        Tracked *one = new Tracked(1);
        Tracked *two = new Tracked(2);
        Tracked *three = new Tracked(3);
        Tracked *four = new Tracked(4);
        CHECK(handler.add(one) == one);
        handler.add(two);
        handler.add(three);
        handler.add(four);
        CHECK(!handler.isEmpty());
        delete two; // forgotten, not deleted again
        CHECK(g_deleted == std::vector<int>{2});
        handler.remove(three);
        delete three; // no longer ours
        CHECK(g_deleted == (std::vector<int>{2, 3}));
    }
    CHECK(g_deleted == (std::vector<int>{2, 3, 4, 1}));

    g_deleted.clear();
    QObjectCleanupHandler handler;
    handler.add(new Tracked(7));
    handler.add(new Tracked(8));
    handler.clear();
    CHECK(handler.isEmpty() && g_deleted == (std::vector<int>{8, 7}));
}

namespace {
class Button : public QObject {
public:
    void click() { emit clicked(); }

signals:
    void clicked() { emitSignal(&Button::clicked); }
};
} // namespace

static void testSignalMapper() {
    QSignalMapper mapper;
    Button a, b, c;
    QObject target;
    mapper.setMapping(&a, 1);
    mapper.setMapping(&b, QStringLiteral("bee"));
    mapper.setMapping(&b, &target);
    mapper.setMapping(&c, 3);
    mapper.setMapping(nullptr, 9);
    CHECK(mapper.mapping(1) == &a && mapper.mapping(3) == &c && mapper.mapping(2) == nullptr);
    CHECK(mapper.mapping(QStringLiteral("bee")) == &b && mapper.mapping(QStringLiteral("x")) == nullptr);
    CHECK(mapper.mapping(&target) == &b);
    for (Button *s : {&a, &b, &c})
        QObject::connect(s, &Button::clicked, &mapper, qOverload<>(&QSignalMapper::map));

    QList<int> ints;
    QStringList strings;
    QList<QObject *> objects;
    QObject::connect(&mapper, &QSignalMapper::mappedInt, [&](int i) { ints.append(i); });
    QObject::connect(&mapper, &QSignalMapper::mappedString, [&](const QString &s) { strings.append(s); });
    QObject::connect(&mapper, &QSignalMapper::mappedObject, [&](QObject *o) { objects.append(o); });
    a.click();
    c.click();
    b.click();
    CHECK(ints == (QList<int>{1, 3}) && strings == QStringList{"bee"} && objects == QList<QObject *>{&target});

    mapper.removeMappings(&c);
    c.click();
    CHECK(ints.size() == 2 && mapper.mapping(3) == nullptr);
    mapper.map(&a); // the explicit overload
    CHECK(ints.size() == 3 && ints.last() == 1);
    mapper.map(nullptr);
    CHECK(ints.size() == 3);

    {
        Button gone;
        mapper.setMapping(&gone, 77);
        CHECK(mapper.mapping(77) == &gone);
    }
    CHECK(mapper.mapping(77) == nullptr); // dropped with its sender
}

static void testEventLoopLocker() {
    {
        QEventLoop loop;
        std::unique_ptr<QEventLoopLocker> first(new QEventLoopLocker(&loop));
        std::unique_ptr<QEventLoopLocker> second(new QEventLoopLocker(&loop));
        QTimer::singleShot(5, &loop, [&] { first.reset(); });
        bool stillRunning = false;
        QTimer::singleShot(40, &loop, [&] {
            stillRunning = loop.isRunning();
            second.reset();
        });
        QTimer::singleShot(3000, &loop, [&] { loop.quit(); });
        QElapsedTimer t;
        t.start();
        loop.exec();
        CHECK(stillRunning && t.elapsed() >= 35 && t.elapsed() < 2000);
    }
    {
        // A locker whose loop died first must not crash.
        auto *loop = new QEventLoop;
        QEventLoopLocker locker(loop);
        delete loop;
    }
}

static void testApplicationLocker(QCoreApplication &app) {
    CHECK(QCoreApplication::isQuitLockEnabled());
    auto locker = std::make_unique<QEventLoopLocker>();
    QTimer::singleShot(5, [&] { locker.reset(); });
    QTimer::singleShot(3000, [] { QCoreApplication::exit(99); });
    CHECK(app.exec() == 0);

    QCoreApplication::setQuitLockEnabled(false);
    CHECK(!QCoreApplication::isQuitLockEnabled());
    locker = std::make_unique<QEventLoopLocker>();
    QTimer::singleShot(5, [&] { locker.reset(); });
    QTimer::singleShot(80, [] { QCoreApplication::exit(7); });
    CHECK(app.exec() == 7);
    QCoreApplication::setQuitLockEnabled(true);
}

static void testSysInfo() {
    CHECK(QSysInfo::WordSize == static_cast<int>(sizeof(void *) * 8));
    CHECK(QSysInfo::kernelType() == "linux" && !QSysInfo::kernelVersion().isEmpty());
    CHECK(!QSysInfo::buildCpuArchitecture().isEmpty() && QSysInfo::buildCpuArchitecture() != "unknown");
    CHECK(QSysInfo::currentCpuArchitecture() == QSysInfo::buildCpuArchitecture());
    const QString abi = QSysInfo::buildAbi();
    CHECK(abi.startsWith(QSysInfo::buildCpuArchitecture()) && abi.contains("little_endian") && abi.endsWith("lp64"));
    CHECK(!QSysInfo::productType().isEmpty() && !QSysInfo::productVersion().isEmpty());
    CHECK(!QSysInfo::prettyProductName().isEmpty());
    const QString host = QSysInfo::machineHostName();
    CHECK(!host.contains('.'));
    const QByteArray boot = QSysInfo::bootUniqueId();
    CHECK(boot.isEmpty() || boot.size() == 36);
    const QByteArray machine = QSysInfo::machineUniqueId();
    CHECK(machine.isEmpty() || machine.size() >= 32);
    CHECK(QSysInfo::bootUniqueId() == boot);
}

static void testOperatingSystemVersion() {
    const QOperatingSystemVersion now = QOperatingSystemVersion::current();
    CHECK(now.type() == QOperatingSystemVersion::Unknown &&
          QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Unknown);
    CHECK(!now.name().isEmpty() && now.isAnyOfType({QOperatingSystemVersion::Unknown}) &&
          !now.isAnyOfType({QOperatingSystemVersion::Windows, QOperatingSystemVersion::Android}));

    const QOperatingSystemVersion v(QOperatingSystemVersion::Windows, 10, 0, 22000);
    CHECK(v.majorVersion() == 10 && v.minorVersion() == 0 && v.microVersion() == 22000 && v.segmentCount() == 3);
    CHECK(v.version() == QVersionNumber(10, 0, 22000) && v.name() == "Windows");
    CHECK(QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 14).segmentCount() == 1);
    CHECK(QOperatingSystemVersion(QOperatingSystemVersion::Android, 13, 1).segmentCount() == 2);
    CHECK(QOperatingSystemVersion(QOperatingSystemVersion::IOS, -1).segmentCount() == 0);
    const QOperatingSystemVersion older(QOperatingSystemVersion::Windows, 6, 3);
    CHECK(older < v && older <= v && v > older && v >= older && v >= v && v <= v && !(v < older));
    const QOperatingSystemVersion other(QOperatingSystemVersion::Android, 99);
    CHECK(!(v < other) && !(v > other) && !(v <= other) && !(v >= other));
    CHECK(QOperatingSystemVersion(QOperatingSystemVersion::TvOS, 1).name() == "tvOS" &&
          QOperatingSystemVersion(QOperatingSystemVersion::VisionOS, 1).name() == "visionOS" &&
          QOperatingSystemVersion(QOperatingSystemVersion::Unknown, -1).name() == "Unknown");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testPartialOrdering();
    testFlag();
    testEndian();
    testGlobalStatic();
    testAtomicRollback();
    testMutableSetIterator();
    testRandom64();
    testFloat16();
    testException();
    testCleanupHandler();
    testSignalMapper();
    testEventLoopLocker();
    testApplicationLocker(app);
    testSysInfo();
    testOperatingSystemVersion();
    if (g_failures == 0)
        std::puts("all core utility tests passed");
    return g_failures == 0 ? 0 : 1;
}
