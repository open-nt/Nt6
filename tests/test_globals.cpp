// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QByteArrayMatcher>
#include <QtCore/QLatin1StringMatcher>
#include <QtCore/QTypeInfo>
#include <QtCore/QtAlgorithms>
#include <QtCore/QtEnvironmentVariables>
#include <QtCore/QtMath>
#include <QtCore/QtNumeric>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool near(double a, double b, double eps = 1e-12) { return std::fabs(a - b) <= eps; }

// The environment

static void testEnvironment() {
    const char *name = "NT_TEST_GLOBALS_VARIABLE";
    CHECK(qunsetenv(name));
    CHECK(!qEnvironmentVariableIsSet(name) && qEnvironmentVariableIsEmpty(name));
    CHECK(qgetenv(name).isNull());
    CHECK(qEnvironmentVariable(name).isNull());
    CHECK(qEnvironmentVariable(name, QString("fallback")) == QString("fallback"));
    bool ok = true;
    CHECK(qEnvironmentVariableIntValue(name, &ok) == 0 && !ok);

    CHECK(qputenv(name, "value"));
    CHECK(qEnvironmentVariableIsSet(name) && !qEnvironmentVariableIsEmpty(name));
    CHECK(qgetenv(name) == QByteArray("value"));
    CHECK(qEnvironmentVariable(name) == QString("value"));
    CHECK(qEnvironmentVariable(name, QString("fallback")) == QString("value"));
    CHECK(qEnvironmentVariableIntValue(name, &ok) == 0 && !ok);
    CHECK(std::string(std::getenv(name)) == "value");

    // A view that does not end with a zero, and text that is not ASCII
    const QByteArray longer("abcdef");
    CHECK(qputenv(name, QByteArrayView(longer.constData(), 3)));
    CHECK(qgetenv(name) == QByteArray("abc"));
    CHECK(qputenv(name, QByteArray("gr\xc3\xbc\xc3\x9f")));
    CHECK(qEnvironmentVariable(name) == QString::fromUtf8("gr\xc3\xbc\xc3\x9f") && qEnvironmentVariable(name).size() == 4);

    // Set, but empty
    CHECK(qputenv(name, QByteArray()));
    CHECK(qEnvironmentVariableIsSet(name) && qEnvironmentVariableIsEmpty(name));
    CHECK(!qgetenv(name).isNull() && qgetenv(name).isEmpty());
    CHECK(qEnvironmentVariable(name, QString("fallback")).isEmpty());
    CHECK(qEnvironmentVariableIntValue(name, &ok) == 0 && !ok);

    // Numbers: decimal, hexadecimal, octal, signs and white space
    const struct {
        const char *text;
        int value;
        bool ok;
    } numbers[] = {
        {"42", 42, true},          {"-17", -17, true},         {"+5", 5, true},           {"0x1f", 31, true},
        {"010", 8, true},          {"  12", 12, true},         {"12  ", 12, true},        {"0", 0, true},
        {"2147483647", 2147483647, true},                      {"-2147483648", -2147483647 - 1, true},
        {"2147483648", 0, false},  {"-2147483649", 0, false},  {"99999999999999999999999", 0, false},
        {"12abc", 0, false},       {"abc", 0, false},          {"1.5", 0, false},         {" ", 0, false},
    };
    for (const auto &n : numbers) {
        CHECK(qputenv(name, n.text));
        ok = !n.ok;
        const int value = qEnvironmentVariableIntValue(name, &ok);
        CHECK(value == n.value && ok == n.ok);
        if (value != n.value || ok != n.ok)
            std::printf("  \"%s\" gave %d, %d\n", n.text, value, int(ok));
        CHECK(qEnvironmentVariableIntValue(name) == n.value);
    }

    CHECK(qunsetenv(name));
    CHECK(!qEnvironmentVariableIsSet(name) && std::getenv(name) == nullptr);
    CHECK(qunsetenv(name)); // removing what is not there succeeds
    CHECK(!qputenv("", "x") && !qputenv("A=B", "x"));
}

// qnumeric.h

template <typename T> static void checkOverflow(T a, T b) {
    T r1 = 0, r2 = 0;
    CHECK(qAddOverflow(a, b, &r1) == QtPrivate::addOverflowGeneric(a, b, &r2) && r1 == r2);
    CHECK(qSubOverflow(a, b, &r1) == QtPrivate::subOverflowGeneric(a, b, &r2) && r1 == r2);
    CHECK(qMulOverflow(a, b, &r1) == QtPrivate::mulOverflowGeneric(a, b, &r2) && r1 == r2);
}

template <typename T> static void checkOverflowOfType() {
    const T lowest = std::numeric_limits<T>::min(), highest = std::numeric_limits<T>::max();
    std::vector<T> values = {0, 1, 2, 3, 7, 15, 16, 100, highest, static_cast<T>(highest - 1), static_cast<T>(highest / 2),
                             static_cast<T>(highest / 2 + 1), static_cast<T>(highest / 3), lowest};
    if (std::numeric_limits<T>::is_signed) {
        for (int v : {-1, -2, -3, -16, -100})
            values.push_back(static_cast<T>(v));
        values.push_back(static_cast<T>(lowest + 1));
        values.push_back(static_cast<T>(lowest / 2));
        values.push_back(static_cast<T>(lowest / 2 - 1));
    }
    for (T a : values) {
        for (T b : values)
            checkOverflow(a, b);
    }
}

static void testNumeric() {
    CHECK(qIsInf(qInf()) && qIsInf(-qInf()) && !qIsInf(1.0) && !qIsInf(qQNaN()));
    CHECK(qIsNaN(qQNaN()) && qIsNaN(qSNaN()) && !qIsNaN(0.0) && !qIsNaN(qInf()));
    CHECK(qIsFinite(1.5) && !qIsFinite(qInf()) && !qIsFinite(qQNaN()));
    CHECK(qIsInf(std::numeric_limits<float>::infinity()) && qIsNaN(std::numeric_limits<float>::quiet_NaN()) &&
          qIsFinite(2.5f));
    CHECK(qFpClassify(0.0) == FP_ZERO && qFpClassify(1.0) == FP_NORMAL && qFpClassify(qInf()) == FP_INFINITE &&
          qFpClassify(qQNaN()) == FP_NAN && qFpClassify(std::numeric_limits<double>::denorm_min()) == FP_SUBNORMAL);
    CHECK(qFpClassify(0.0f) == FP_ZERO && qFpClassify(1.0f) == FP_NORMAL);
    static_assert(qInf() > std::numeric_limits<double>::max());

    // The distance counts the numbers on the way
    CHECK(qFloatDistance(1.0, 1.0) == 0 && qFloatDistance(1.0f, 1.0f) == 0);
    CHECK(qFloatDistance(1.0, std::nextafter(1.0, 2.0)) == 1 && qFloatDistance(std::nextafter(1.0, 2.0), 1.0) == 1);
    CHECK(qFloatDistance(1.0f, std::nextafter(1.0f, 2.0f)) == 1);
    CHECK(qFloatDistance(0.0, -0.0) == 0 && qFloatDistance(0.0f, -0.0f) == 0);
    const double tiny = std::numeric_limits<double>::denorm_min();
    CHECK(qFloatDistance(0.0, tiny) == 1 && qFloatDistance(-tiny, tiny) == 2 && qFloatDistance(tiny, -tiny) == 2);
    CHECK(qFloatDistance(1.0f, 2.0f) == (quint32(1) << 23) && qFloatDistance(1.0, 2.0) == (quint64(1) << 52));
    CHECK(qFloatDistance(-1.0f, -2.0f) == (quint32(1) << 23));
    double walked = 1.0;
    for (int i = 0; i < 1000; ++i)
        walked = std::nextafter(walked, 0.0);
    CHECK(qFloatDistance(1.0, walked) == 1000);

    // Overflow
    int i = 0;
    CHECK(!qAddOverflow(2, 3, &i) && i == 5);
    CHECK(qAddOverflow(std::numeric_limits<int>::max(), 1, &i) && i == std::numeric_limits<int>::min());
    CHECK(!qSubOverflow(2, 3, &i) && i == -1);
    CHECK(qSubOverflow(std::numeric_limits<int>::min(), 1, &i) && i == std::numeric_limits<int>::max());
    CHECK(!qMulOverflow(-4, 5, &i) && i == -20);
    CHECK(qMulOverflow(65536, 65536, &i) && i == 0);
    CHECK(qMulOverflow(std::numeric_limits<int>::min(), -1, &i));
    uint u = 0;
    CHECK(qSubOverflow(2u, 3u, &u) && u == std::numeric_limits<uint>::max());
    CHECK(qAddOverflow(std::numeric_limits<uint>::max(), 1u, &u) && u == 0);
    CHECK(!qMulOverflow(65535u, 65537u, &u) && u == std::numeric_limits<uint>::max());
    qsizetype size = 0;
    CHECK(qMulOverflow(std::numeric_limits<qsizetype>::max() / 2, qsizetype(3), &size));
    CHECK(!qAddOverflow(qsizetype(40), qsizetype(2), &size) && size == 42);

    // The version without compiler support agrees with the one with it, for every width
    checkOverflowOfType<signed char>();
    checkOverflowOfType<unsigned char>();
    checkOverflowOfType<short>();
    checkOverflowOfType<unsigned short>();
    checkOverflowOfType<int>();
    checkOverflowOfType<unsigned int>();
    checkOverflowOfType<long>();
    checkOverflowOfType<unsigned long>();
    checkOverflowOfType<long long>();
    checkOverflowOfType<unsigned long long>();
    static_assert([] {
        int r = 0;
        return QtPrivate::mulOverflowGeneric(46341, 46341, &r) && !QtPrivate::mulOverflowGeneric(46340, 46340, &r);
    }());
}

// qalgorithms.h

template <typename T> static void checkBitsOfType() {
    const uint width = static_cast<uint>(std::numeric_limits<T>::digits);
    CHECK(qPopulationCount(T(0)) == 0 && qCountTrailingZeroBits(T(0)) == width && qCountLeadingZeroBits(T(0)) == width);
    CHECK(qPopulationCount(static_cast<T>(~T(0))) == width);
    CHECK(qCountTrailingZeroBits(static_cast<T>(~T(0))) == 0 && qCountLeadingZeroBits(static_cast<T>(~T(0))) == 0);
    for (uint bit = 0; bit < width; ++bit) {
        const T one = static_cast<T>(T(1) << bit);
        CHECK(qPopulationCount(one) == 1);
        CHECK(qCountTrailingZeroBits(one) == bit);
        CHECK(qCountLeadingZeroBits(one) == width - 1 - bit);
        // Bits above the lowest do not change the trailing zeros, and bits below the highest not the leading ones
        const T above = static_cast<T>(~0ULL << bit), below = static_cast<T>(one | (one - 1));
        CHECK(qCountTrailingZeroBits(above) == bit && qPopulationCount(above) == width - bit);
        CHECK(qCountLeadingZeroBits(below) == width - 1 - bit && qPopulationCount(below) == bit + 1);
    }
}

static void testBits() {
    checkBitsOfType<unsigned char>();
    checkBitsOfType<unsigned short>();
    checkBitsOfType<unsigned int>();
    checkBitsOfType<unsigned long>();
    checkBitsOfType<unsigned long long>();
    // The Qt integer types pick an overload, whatever they are on the platform
    CHECK(qPopulationCount(quint8(0xf0)) == 4 && qPopulationCount(quint16(0xf0f0)) == 8);
    CHECK(qPopulationCount(quint32(0xdeadbeef)) == 24 && qPopulationCount(quint64(0xdeadbeefdeadbeefULL)) == 48);
    CHECK(qCountTrailingZeroBits(quint32(0x80)) == 7 && qCountTrailingZeroBits(quint64(1) << 40) == 40);
    CHECK(qCountLeadingZeroBits(quint8(1)) == 7 && qCountLeadingZeroBits(quint16(1)) == 15);
    CHECK(qCountLeadingZeroBits(quint32(1)) == 31 && qCountLeadingZeroBits(quint64(1)) == 63);
    static_assert(qPopulationCount(0x12345678u) == 13);
    static_assert(qCountTrailingZeroBits(0x1000u) == 12 && qCountLeadingZeroBits(0x1000u) == 19);
}

// qmath.h

static void testMath() {
    CHECK(qCeil(1.2) == 2 && qCeil(-1.2) == -1 && qCeil(3.0) == 3 && qCeil(0.1f) == 1);
    CHECK(qFloor(1.8) == 1 && qFloor(-1.2) == -2 && qFloor(3.0) == 3 && qFloor(-0.1f) == -1);
    CHECK(qFabs(-2.5) == 2.5 && qFabs(2.5f) == 2.5f);
    CHECK(near(qSin(M_PI_2), 1.0) && near(qCos(M_PI), -1.0) && near(qTan(M_PI_4), 1.0));
    CHECK(near(qAsin(1.0), M_PI_2) && near(qAcos(-1.0), M_PI) && near(qAtan(1.0), M_PI_4));
    CHECK(near(qAtan2(1.0, -1.0), 3 * M_PI_4) && near(qAtan2(-1.0, 0.0), -M_PI_2));
    CHECK(qSqrt(81.0) == 9.0 && near(qLn(M_E), 1.0) && near(qExp(1.0), M_E) && qPow(2.0, 10) == 1024.0);
    CHECK(qHypot(3.0, 4.0) == 5.0 && near(qHypot(2.0, 3.0, 6.0), 7.0));
    CHECK(near(qFastSin(1.0), std::sin(1.0)) && near(qFastCos(1.0), std::cos(1.0)));
    // A float stays a float
    static_assert(std::is_same_v<decltype(qSin(1.0f)), float> && std::is_same_v<decltype(qSqrt(2.0)), double>);
    static_assert(std::is_same_v<decltype(qSqrt(2)), double> && std::is_same_v<decltype(qCeil(2.5f)), int>);

    CHECK(near(M_PI, 4 * std::atan(1.0)) && near(M_E, std::exp(1.0)) && near(M_SQRT2, std::sqrt(2.0)));
    CHECK(near(M_LN2, std::log(2.0)) && near(M_LN10, std::log(10.0)) && near(M_LOG2E, 1 / std::log(2.0)));
    CHECK(near(M_LOG10E, 1 / std::log(10.0)) && near(M_1_PI, 1 / M_PI) && near(M_2_PI, 2 / M_PI));
    CHECK(near(M_2_SQRTPI, 2 / std::sqrt(M_PI)) && near(M_SQRT1_2, std::sqrt(0.5)));

    CHECK(near(qDegreesToRadians(180.0), M_PI) && near(qRadiansToDegrees(M_PI), 180.0));
    CHECK(std::fabs(qDegreesToRadians(90.0f) - float(M_PI_2)) < 1e-6f && std::fabs(qRadiansToDegrees(float(M_PI)) - 180.0f) < 1e-4f);
    CHECK(near(qDegreesToRadians(90), M_PI_2) && near(static_cast<double>(qDegreesToRadians(45.0L)), M_PI_4));
    CHECK(near(static_cast<double>(qRadiansToDegrees(static_cast<long double>(M_PI_2))), 90.0));
    static_assert(std::is_same_v<decltype(qDegreesToRadians(1.0f)), float> &&
                  std::is_same_v<decltype(qDegreesToRadians(1)), double> &&
                  std::is_same_v<decltype(qRadiansToDegrees(1.0L)), long double>);
    static_assert(qDegreesToRadians(0.0) == 0.0);
    for (double degrees : {-720.0, -90.0, 0.0, 1.0, 33.3, 360.0})
        CHECK(near(qRadiansToDegrees(qDegreesToRadians(degrees)), degrees, 1e-9));

    // The next power of two is greater than the number
    CHECK(qNextPowerOfTwo(0u) == 1 && qNextPowerOfTwo(1u) == 2 && qNextPowerOfTwo(2u) == 4 && qNextPowerOfTwo(3u) == 4);
    CHECK(qNextPowerOfTwo(4u) == 8 && qNextPowerOfTwo(5u) == 8 && qNextPowerOfTwo(7u) == 8 && qNextPowerOfTwo(8u) == 16);
    CHECK(qNextPowerOfTwo(0) == 1 && qNextPowerOfTwo(17) == 32 && qNextPowerOfTwo(-1) == 0 && qNextPowerOfTwo(-100) == 0);
    CHECK(qNextPowerOfTwo(quint32(0x7fffffff)) == 0x80000000u && qNextPowerOfTwo(quint32(0x80000000)) == 0);
    CHECK(qNextPowerOfTwo(qint32(0x3fffffff)) == 0x40000000 && qNextPowerOfTwo(qint32(0x40000000)) == 0);
    CHECK(qNextPowerOfTwo(quint64(1) << 40) == (quint64(1) << 41) && qNextPowerOfTwo(qint64(1) << 40) == (qint64(1) << 41));
    CHECK(qNextPowerOfTwo((quint64(1) << 63) - 1) == (quint64(1) << 63) && qNextPowerOfTwo(quint64(1) << 63) == 0);
    CHECK(qNextPowerOfTwo(std::numeric_limits<qint64>::max()) == 0 && qNextPowerOfTwo(qint64(-5)) == 0);
    CHECK(qNextPowerOfTwo(5LL) == 8 && qNextPowerOfTwo(5ULL) == 8 && qNextPowerOfTwo(5L) == 8 && qNextPowerOfTwo(5UL) == 8);
    static_assert(std::is_same_v<decltype(qNextPowerOfTwo(quint32(1))), quint32> &&
                  std::is_same_v<decltype(qNextPowerOfTwo(qint64(1))), qint64> &&
                  std::is_same_v<decltype(qNextPowerOfTwo(short(1))), int>);
    static_assert(qNextPowerOfTwo(1000u) == 1024 && qNextPowerOfTwo(short(300)) == 512);
}

// The macros and small functions of qglobal.h

namespace {

struct Pinned {
    Pinned() = default;
    Q_DISABLE_COPY_MOVE(Pinned)
};
struct OnlyCopies {
    OnlyCopies() = default;
    OnlyCopies(const OnlyCopies &) = default;
    OnlyCopies &operator=(const OnlyCopies &) = default;
    Q_DISABLE_MOVE(OnlyCopies)
};
static_assert(!std::is_copy_constructible_v<Pinned> && !std::is_move_constructible_v<Pinned>);
static_assert(!std::is_copy_assignable_v<Pinned> && !std::is_move_assignable_v<Pinned>);
static_assert(std::is_copy_assignable_v<OnlyCopies>);

struct Plain {
    int a, b;
};
struct WithString {
    std::string text;
};
struct Declared {
    std::string text;
};
struct Flagged {
    Flagged() {}
    int value = 0;
};

} // namespace

Q_DECLARE_TYPEINFO(Declared, Q_RELOCATABLE_TYPE);
Q_DECLARE_TYPEINFO(Flagged, Q_PRIMITIVE_TYPE);

QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(NtForwardClass)
QT_FORWARD_DECLARE_STRUCT(NtForwardStruct)
QT_END_NAMESPACE
QT_USE_NAMESPACE

class NtForwardClass {
public:
    int value = 7;
};
struct NtForwardStruct {
    int value = 8;
};

namespace {

class Shape {
public:
    virtual ~Shape() = default;
};
class Printable {
public:
    virtual ~Printable() = default;
    virtual int pages() const = 0;
};
class Unrelated {
public:
    virtual ~Unrelated() = default;
};

} // namespace

Q_DECLARE_INTERFACE(Printable, "org.nt.test.Printable/1.0")

namespace {

class Document : public Shape, public Printable {
public:
    int pages() const override { return 3; }
};

Q_DECL_CONSTEXPR int doubled(int v) Q_DECL_NOEXCEPT { return 2 * v; }
Q_REQUIRED_RESULT Q_ALWAYS_INLINE int inlined(int v) { return v + 1; }
Q_NEVER_INLINE Q_DECL_UNUSED int notInlined(int v) { return v + 2; }

int classify(int v) {
    int out = 0;
    switch (v) {
    case 0:
        out += 1;
        Q_FALLTHROUGH();
    case 1:
        out += 10;
        break;
    case 2:
        return 100;
    default:
        Q_UNREACHABLE_RETURN(-1);
    }
    return out;
}

int sign(int v) {
    Q_ASSUME(v != 0);
    if (v > 0)
        return 1;
    if (v < 0)
        return -1;
    Q_UNREACHABLE();
}

int postOrder[8];
int postCount = 0;
int preCalls = 0;
void postA() { postOrder[postCount++] = 1; }
void postB() { postOrder[postCount++] = 2; }
void postC() { postOrder[postCount++] = 3; }
void postRemoved() { postOrder[postCount++] = 99; }
// A routine may register another while the routines run.
void postD() {
    postOrder[postCount++] = 4;
    qAddPostRoutine(postC);
}
void preRoutine() { ++preCalls; }
int startupCalls = 0;
bool startupSawApplication = false;
void startupFunction() {
    ++startupCalls;
    startupSawApplication = QCoreApplication::instance() != nullptr;
}
Q_COREAPP_STARTUP_FUNCTION(startupFunction)

Q_STATIC_LOGGING_CATEGORY(lcStatic, "nt.test.static")
Q_STATIC_LOGGING_CATEGORY(lcStaticWarning, "nt.test.static.warning", QtWarningMsg)

QtMsgType g_lastType = QtDebugMsg;
QString g_lastMessage;
int g_messages = 0;
void capture(QtMsgType type, const QMessageLogContext &, const QString &message) {
    g_lastType = type;
    g_lastMessage = message;
    ++g_messages;
}

} // namespace

static void testMacros() {
    CHECK(std::string(QT_STRINGIFY(QT_VERSION_MAJOR)) == std::to_string(QT_VERSION_MAJOR));
    CHECK(std::string(QT_STRINGIFY2(QT_VERSION_MAJOR)) == "QT_VERSION_MAJOR");
    CHECK(std::string(QT_VERSION_STR) == qVersion());

    static_assert(doubled(21) == 42);
    Q_STATIC_ASSERT(sizeof(qint64) == 8);
    Q_STATIC_ASSERT_X(sizeof(qint16) == 2, "qint16 has two bytes");
    CHECK(inlined(1) == 2 && notInlined(1) == 3);
    Q_ASSERT_X(doubled(1) == 2, "testMacros()", "doubling works");
    qt_noop();

    CHECK(classify(0) == 11 && classify(1) == 10 && classify(2) == 100);
    CHECK(sign(5) == 1 && sign(-5) == -1);

    int loops = 0;
    Q_FOREVER {
        if (++loops == 3)
            break;
    }
    CHECK(loops == 3);

    // Q_CHECK_PTR() passes a pointer and throws for none
    int value = 0;
    int *pointer = &value;
    Q_CHECK_PTR(pointer);
    bool thrown = false;
    try {
        pointer = nullptr;
        Q_CHECK_PTR(pointer);
    } catch (const std::bad_alloc &) {
        thrown = true;
    }
    CHECK(thrown);
    thrown = false;
    try {
        qBadAlloc();
    } catch (const std::bad_alloc &) {
        thrown = true;
    }
    CHECK(thrown);
    static_assert(noexcept(qTerminate()));

    // qAsConst() and qExchange()
    std::vector<int> numbers = {1, 2, 3};
    static_assert(std::is_same_v<decltype(qAsConst(numbers)), const std::vector<int> &>);
    CHECK(&qAsConst(numbers) == &numbers);
    int sum = 0;
    for (int n : qAsConst(numbers))
        sum += n;
    CHECK(sum == 6);
    int state = 5;
    CHECK(qExchange(state, 9) == 5 && state == 9);
    std::string text = "before";
    CHECK(qExchange(text, "after") == "before" && text == "after");
    std::vector<int> taken = qExchange(numbers, {});
    CHECK(taken.size() == 3 && numbers.empty());
    static_assert([] {
        int v = 1;
        return qExchange(v, 2) == 1 && v == 2;
    }());

    // Forward declarations and the namespace macros
    QT_PREPEND_NAMESPACE(NtForwardClass) forwardClass;
    NtForwardStruct forwardStruct;
    CHECK(forwardClass.value == 7 && forwardStruct.value == 8);

    // Interfaces
    CHECK(std::string(qobject_interface_iid<Printable *>()) == "org.nt.test.Printable/1.0");
    CHECK(qobject_interface_iid<Shape *>() == nullptr);
    Document document;
    Shape *shape = &document;
    Printable *printable = qobject_cast<Printable *>(shape);
    CHECK(printable && printable->pages() == 3);
    CHECK(qobject_cast<Unrelated *>(shape) == nullptr);

    // QTypeInfo
    CHECK(!QTypeInfo<int>::isComplex && QTypeInfo<int>::isRelocatable && QTypeInfo<int>::isIntegral);
    CHECK(!QTypeInfo<int>::isPointer && QTypeInfo<int *>::isPointer && !QTypeInfo<int *>::isIntegral);
    CHECK(!QTypeInfo<Plain>::isComplex && QTypeInfo<Plain>::isRelocatable && !QTypeInfo<Plain>::isIntegral);
    CHECK(QTypeInfo<WithString>::isComplex && !QTypeInfo<WithString>::isRelocatable);
    CHECK(QTypeInfo<Declared>::isComplex && QTypeInfo<Declared>::isRelocatable);
    CHECK(!QTypeInfo<Flagged>::isComplex && QTypeInfo<Flagged>::isRelocatable);
    CHECK(Q_MOVABLE_TYPE == Q_RELOCATABLE_TYPE && Q_COMPLEX_TYPE == 0 && Q_DUMMY_TYPE == 0);
}

static void testLogging() {
    CHECK(std::string(lcStatic().categoryName()) == "nt.test.static" && lcStatic().isDebugEnabled());
    CHECK(!lcStaticWarning().isDebugEnabled() && lcStaticWarning().isWarningEnabled());
    CHECK(&lcStatic() == &lcStatic());

    CHECK(qt_error_string(ENOENT) == QString::fromLocal8Bit(std::strerror(ENOENT)));
    errno = EACCES;
    CHECK(qt_error_string() == QString::fromLocal8Bit(std::strerror(EACCES)));
    CHECK(qt_error_string(0) == QString::fromLocal8Bit(std::strerror(0)));

    const QtMessageHandler previous = qInstallMessageHandler(capture);
    errno = ENOENT;
    qErrnoWarning("cannot open %s (%d)", "the file", 7);
    CHECK(g_messages == 1 && g_lastType == QtCriticalMsg);
    CHECK(g_lastMessage == QString("cannot open the file (7) (") + qt_error_string(ENOENT) + QString(")"));
    errno = 0;
    qErrnoWarning(EEXIST, "cannot make %s", "the directory");
    CHECK(g_messages == 2 && g_lastType == QtCriticalMsg);
    CHECK(g_lastMessage == QString("cannot make the directory (") + qt_error_string(EEXIST) + QString(")"));
    qInstallMessageHandler(previous);
}

// Routines of the application

static void testRoutines(int &argc, char **argv) {
    // Registered before there is an application: called by each constructor, in the order they were added
    CHECK(startupCalls == 0 && preCalls == 0);
    qAddPreRoutine(preRoutine);
    qAddPreRoutine(nullptr);
    CHECK(preCalls == 0);
    qAddPostRoutine(postA);
    qAddPostRoutine(postRemoved);
    qAddPostRoutine(postB);
    qAddPostRoutine(postRemoved);
    qAddPostRoutine(nullptr);
    {
        QCoreApplication app(argc, argv);
        CHECK(startupCalls == 1 && startupSawApplication && preCalls == 1);
        // With an application the routine is called at once
        qAddPreRoutine(preRoutine);
        CHECK(preCalls == 2);
        qRemovePostRoutine(postRemoved); // both entries
        qRemovePostRoutine(postC);       // not there
        qAddPostRoutine(postD);
        CHECK(postCount == 0);
    }
    // Last added first; the routine that postD() added ran too
    CHECK(postCount == 4 && postOrder[0] == 4 && postOrder[1] == 3 && postOrder[2] == 2 && postOrder[3] == 1);
    {
        // The post routines ran once and are gone; the pre routines stay for the next application
        QCoreApplication app(argc, argv);
        CHECK(startupCalls == 2 && preCalls == 4);
    }
    CHECK(postCount == 4);
}

// QByteArrayMatcher, QStaticByteArrayMatcher and QLatin1StringMatcher

static void testByteArrayMatcher() {
    QByteArrayMatcher empty;
    CHECK(empty.pattern().isEmpty());
    CHECK(empty.indexIn(QByteArray("abc")) == 0 && empty.indexIn(QByteArray("abc"), 2) == 2);
    CHECK(empty.indexIn(QByteArray("abc"), 3) == 3 && empty.indexIn(QByteArray("abc"), 4) == -1);
    CHECK(empty.indexIn(QByteArray()) == 0);

    QByteArrayMatcher matcher(QByteArray("needle"));
    CHECK(matcher.pattern() == QByteArray("needle"));
    const QByteArray hay("a needle in a haystack of needles, needle");
    CHECK(matcher.indexIn(hay) == 2 && matcher.indexIn(hay, 2) == 2 && matcher.indexIn(hay, 3) == 26);
    CHECK(matcher.indexIn(hay, 27) == 35 && matcher.indexIn(hay, 36) == -1 && matcher.indexIn(hay, 1000) == -1);
    CHECK(matcher.indexIn(hay, -5) == 2);
    CHECK(matcher.indexIn(hay.constData(), 7) == -1 && matcher.indexIn(hay.constData(), 8) == 2);
    CHECK(matcher.indexIn(QByteArrayView(hay.constData() + 3, hay.size() - 3)) == 23);
    CHECK(matcher.indexIn("no such thing") == -1 && matcher.indexIn(QByteArray()) == -1 && matcher.indexIn("needl") == -1);

    // The other constructors, and a pattern with a zero byte
    CHECK(QByteArrayMatcher("needle").indexIn(hay) == 2);
    CHECK(QByteArrayMatcher("needleXYZ", 6).indexIn(hay) == 2);
    CHECK(QByteArrayMatcher(QByteArrayView(hay.constData() + 2, 6)).pattern() == QByteArray("needle"));
    const QByteArray zeros("ab\0cd\0\0ef", 9);
    CHECK(QByteArrayMatcher(QByteArray("\0\0", 2)).indexIn(zeros) == 5);
    CHECK(QByteArrayMatcher("d\0\0e", 4).indexIn(zeros) == 4);

    // Copies, and a new pattern
    QByteArrayMatcher copy(matcher);
    matcher.setPattern(QByteArray("hay"));
    CHECK(matcher.pattern() == QByteArray("hay") && matcher.indexIn(hay) == 14);
    CHECK(copy.pattern() == QByteArray("needle") && copy.indexIn(hay) == 2);
    copy = matcher;
    CHECK(copy.indexIn(hay) == 14);

    // Bytes above 127, a single byte, and the pattern at the end
    CHECK(QByteArrayMatcher("\xff\xfe").indexIn(QByteArray("ab\xff\xff\xfe")) == 3);
    CHECK(QByteArrayMatcher("x").indexIn("aaax") == 3 && QByteArrayMatcher("x").indexIn("aaa") == -1);
    CHECK(QByteArrayMatcher("aab").indexIn("aaaaaab") == 4);

    // A pattern longer than the largest shift of the table, and every position against the plain search
    QByteArray longPattern;
    for (int i = 0; i < 700; ++i)
        longPattern.append(static_cast<char>('a' + i % 23));
    QByteArray longHay(2000, 'z');
    longHay.append(longPattern.left(699));
    longHay.append(QByteArray(300, 'q'));
    longHay.append(longPattern);
    longHay.append("tail");
    CHECK(QByteArrayMatcher(longPattern).indexIn(longHay) == 2000 + 699 + 300);
    quint32 seed = 12345;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 16;
    };
    for (int round = 0; round < 300; ++round) {
        QByteArray text, pattern;
        const int textSize = static_cast<int>(next() % 60), patternSize = static_cast<int>(next() % 5);
        for (int i = 0; i < textSize; ++i)
            text.append(static_cast<char>('a' + next() % 3));
        for (int i = 0; i < patternSize; ++i)
            pattern.append(static_cast<char>('a' + next() % 3));
        const QByteArrayMatcher m(pattern);
        for (qsizetype from = 0; from <= text.size() + 1; ++from) {
            const qsizetype expected = from > text.size() ? -1 : text.indexOf(pattern, from);
            CHECK(m.indexIn(text, from) == expected);
        }
    }
}

static void testStaticByteArrayMatcher() {
    static constexpr auto matcher = qMakeStaticByteArrayMatcher("needle");
    static_assert(std::is_same_v<decltype(matcher), const QStaticByteArrayMatcher<7>>);
    const QByteArray hay("a needle in a haystack of needles");
    CHECK(matcher.pattern() == QByteArray("needle"));
    CHECK(matcher.indexIn(hay) == 2 && matcher.indexIn(hay, 3) == 26 && matcher.indexIn(hay, 27) == -1);
    CHECK(matcher.indexIn(hay.constData(), 7) == -1 && matcher.indexIn(hay.constData(), hay.size(), 1) == 2);
    CHECK(matcher.indexIn(QByteArrayView("needle")) == 0 && matcher.indexIn("nothing") == -1);
    // The search itself can run when the program is compiled
    static_assert(matcher.indexIn("a needle", 8) == 2 && matcher.indexIn("a needl", 7) == -1);
    static_assert(qMakeStaticByteArrayMatcher("").indexIn("abc", 3) == 0);
    static_assert(qMakeStaticByteArrayMatcher("ab").indexIn("aaab", 4) == 2);
    constexpr QStaticByteArrayMatcher<3> explicitSize("zz");
    CHECK(explicitSize.indexIn(QByteArray("buzz")) == 2 && explicitSize.pattern().size() == 2);
}

static void testLatin1StringMatcher() {
    using namespace Qt::StringLiterals;
    QLatin1StringMatcher empty;
    CHECK(empty.pattern().isEmpty() && empty.caseSensitivity() == Qt::CaseSensitive);
    CHECK(empty.indexIn("abc"_L1) == 0 && empty.indexIn("abc"_L1, 3) == 3 && empty.indexIn("abc"_L1, 4) == -1);
    CHECK(empty.indexIn(QStringView(u"abc")) == 0 && empty.indexIn(QStringView()) == 0);

    QLatin1StringMatcher matcher("Needle"_L1);
    CHECK(matcher.pattern() == "Needle"_L1);
    const QLatin1StringView hay("a needle, a Needle, a NEEDLE and a Needle");
    CHECK(matcher.indexIn(hay) == 12 && matcher.indexIn(hay, 13) == 35 && matcher.indexIn(hay, 36) == -1);
    CHECK(matcher.indexIn(hay, -3) == 12);
    const QString wide = QString::fromLatin1(hay.data(), hay.size());
    CHECK(matcher.indexIn(QStringView(wide)) == 12 && matcher.indexIn(QStringView(wide), 13) == 35);

    matcher.setCaseSensitivity(Qt::CaseInsensitive);
    CHECK(matcher.caseSensitivity() == Qt::CaseInsensitive);
    CHECK(matcher.indexIn(hay) == 2 && matcher.indexIn(hay, 3) == 12 && matcher.indexIn(hay, 13) == 22);
    CHECK(matcher.indexIn(QStringView(wide), 23) == 35 && matcher.indexIn(QStringView(wide), 36) == -1);
    matcher.setPattern("HAY"_L1);
    CHECK(matcher.pattern() == "HAY"_L1 && matcher.indexIn("a haystack"_L1) == 2);
    matcher.setCaseSensitivity(Qt::CaseSensitive);
    CHECK(matcher.indexIn("a haystack"_L1) == -1);

    // The letters of Latin-1 fold; the multiplication and division signs are not a pair
    const QLatin1StringMatcher umlaut(QLatin1StringView("\xc4\xd6\xdc"), Qt::CaseInsensitive); // upper case a, o, u with dots
    CHECK(umlaut.indexIn(QLatin1StringView("x\xe4\xf6\xfc")) == 1 && umlaut.indexIn(QLatin1StringView("\xc4\xd6\xdc")) == 0);
    CHECK(umlaut.indexIn(QStringView(u"xxäöü")) == 2);
    CHECK(QLatin1StringMatcher(QLatin1StringView("\xc4\xd6\xdc")).indexIn(QLatin1StringView("x\xe4\xf6\xfc")) == -1);
    CHECK(QLatin1StringMatcher(QLatin1StringView("\xd7"), Qt::CaseInsensitive).indexIn(QLatin1StringView("\xf7")) == -1);
    CHECK(QLatin1StringMatcher(QLatin1StringView("\xf7"), Qt::CaseInsensitive).indexIn(QLatin1StringView("\xd7\xf7")) == 1);

    // Characters outside Latin-1 match nothing, also where their low byte is in the pattern
    const QLatin1StringMatcher ab("ab"_L1, Qt::CaseInsensitive);
    CHECK(ab.indexIn(QStringView(u"šŢab")) == 2 && ab.indexIn(QStringView(u"šŢ")) == -1);
    CHECK(ab.indexIn(QStringView(u"aŢŁłAB")) == 4);
    CHECK(QLatin1StringMatcher("k"_L1, Qt::CaseInsensitive).indexIn(QStringView(u"K")) == -1); // the Kelvin sign

    // Every position against the plain search, in both kinds of text
    quint32 seed = 99;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 16;
    };
    const char letters[] = "aAbB";
    for (int round = 0; round < 300; ++round) {
        QByteArray text, pattern;
        const int textSize = static_cast<int>(next() % 50), patternSize = static_cast<int>(next() % 4);
        for (int i = 0; i < textSize; ++i)
            text.append(letters[next() % 4]);
        for (int i = 0; i < patternSize; ++i)
            pattern.append(letters[next() % 4]);
        const QString wideText = QString::fromLatin1(text), widePattern = QString::fromLatin1(pattern);
        for (Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
            const QLatin1StringMatcher m(QLatin1StringView(pattern), cs);
            for (qsizetype from = 0; from <= text.size() + 1; ++from) {
                const qsizetype expected = from > text.size() ? -1 : wideText.indexOf(widePattern, from, cs);
                CHECK(m.indexIn(QLatin1StringView(text), from) == expected);
                CHECK(m.indexIn(QStringView(wideText), from) == expected);
            }
        }
    }
}

int main(int argc, char **argv) {
    testEnvironment();
    testNumeric();
    testBits();
    testMath();
    testMacros();
    testLogging();
    testRoutines(argc, argv);
    testByteArrayMatcher();
    testStaticByteArrayMatcher();
    testLatin1StringMatcher();

    if (g_failures) {
        std::printf("%d globals test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all globals tests passed");
    return 0;
}
