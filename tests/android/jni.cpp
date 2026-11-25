// MIT License - see LICENSE
//
// QJniObject, QJniEnvironment and QNativeInterface::QAndroidApplication on a device: install the APK, start it and
// read logcat (tag Nt). It ends with "JNI tests: PASSED" or "JNI tests: FAILED", and then quits.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QThread>

#include <memory>

using QNativeInterface::QAndroidApplication;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            qWarning("FAIL %s:%d  %s", __FILE__, __LINE__, #cond);                                                     \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testApplication() {
    const QJniObject context = QAndroidApplication::context();
    CHECK(context.isValid());
    CHECK(QAndroidApplication::isActivityContext());
    CHECK(context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString() ==
          QStringLiteral("org.nt.nt_android_jni"));
    CHECK(context.callObjectMethod<jstring>("getPackageName").toString().startsWith(QStringLiteral("org.nt.")));
    CHECK(QAndroidApplication::sdkVersion() == QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT"));
    CHECK(context.className().endsWith("Activity"));
}

static void testStringsAndMethods() {
    const QString text = QString::fromUtf8("h\xc3\xa9llo \xf0\x9f\x8c\xb0"); // with a character outside the BMP
    const QJniObject string = QJniObject::fromString(text);
    CHECK(string.isValid());
    CHECK(string.toString() == text);
    CHECK(string.className() == "java/lang/String");
    CHECK(string.callMethod<jint>("length") == static_cast<jint>(text.size()));
    CHECK(!string.callMethod<bool>("isEmpty"));
    CHECK(string.callMethod<jchar>("charAt", jint(0)) == 'h');
    CHECK(string.callMethod<jint>("indexOf", QStringLiteral("llo")) == 2);
    CHECK(string.callMethod<QString>("substring", jint(0), jint(2)) == text.left(2));

    CHECK(QJniObject::callStaticMethod<jint>("java/lang/Math", "max", jint(3), jint(9)) == 9);
    CHECK(QJniObject::callStaticMethod<jdouble>("java.lang.Math", "sqrt", 16.0) == 4.0);
    const QJniObject number = QJniObject::fromString(QStringLiteral("1234"));
    CHECK(QJniObject::callStaticMethod<jint>("java/lang/Integer", "parseInt", "(Ljava/lang/String;)I",
                                             number.object<jstring>()) == 1234);
    CHECK(QJniObject::callStaticObjectMethod("java/lang/String", "valueOf", "(I)Ljava/lang/String;", 77).toString() ==
          QStringLiteral("77"));

    QJniObject builder("java/lang/StringBuilder", "(Ljava/lang/String;)V", number.object<jstring>());
    builder.callObjectMethod("append", "(J)Ljava/lang/StringBuilder;", jlong(5678));
    CHECK(builder.toString() == QStringLiteral("12345678"));
}

static void testFields() {
    QJniObject point("android/graphics/Point", "(II)V", 2, 5);
    CHECK(point.isValid());
    CHECK(point.getField<jint>("x") == 2 && point.getField<jint>("y") == 5);
    point.setField("x", jint(11));
    CHECK(point.getField<jint>("x") == 11);
    CHECK(QJniObject::getStaticObjectField("android/os/Build", "MODEL", "Ljava/lang/String;").isValid());
    CHECK(!QJniObject::getStaticField<QString>("android/os/Build", "MANUFACTURER").isEmpty());
    const QJniObject other = point;
    CHECK(other == point);
    CHECK(other != QJniObject("android/graphics/Point"));
}

// Failures are reported and cleared: nothing crashes, and the next call works.
static void testFailures() {
    CHECK(!QJniObject("org/nt/DoesNotExist").isValid());
    CHECK(!QJniObject::isClassAvailable("org/nt/DoesNotExist"));
    CHECK(QJniObject::fromString(QStringLiteral("x")).callMethod<jint>("noSuchMethod") == 0);
    CHECK(QJniObject::callStaticMethod<jint>("java/lang/Integer", "parseInt", QStringLiteral("zz")) == 0);
    CHECK(QJniObject::callStaticMethod<jint>("java/lang/Integer", "parseInt", QStringLiteral("42")) == 42);
    CHECK(!QJniObject().isValid());
    CHECK(QJniObject().callMethod<jint>("length") == 0);
    CHECK(QJniObject().toString().isEmpty());
    QJniEnvironment env;
    CHECK(env.isValid());
    CHECK(!env.checkAndClearExceptions());
    jclass stringClass = env.findClass("java/lang/String");
    CHECK(stringClass && env.findMethod(stringClass, "length", "()I"));
    CHECK(!env.findMethod(stringClass, "nothing", "()V"));
}

// A thread that Java did not start is attached on use, and still finds the classes of the package.
static void testThreads() {
    bool foundApplicationClass = false;
    bool called = false;
    std::unique_ptr<QThread> thread(QThread::create([&] {
        foundApplicationClass = QJniObject::isClassAvailable("org/nt/NtActivity");
        called = QJniObject::callStaticMethod<jint>("java/lang/Math", "abs", jint(-5)) == 5;
    }));
    thread->start();
    thread->wait();
    CHECK(foundApplicationClass);
    CHECK(called);

    const auto onMainThread = [] {
        const QJniObject mine =
            QJniObject::callStaticObjectMethod("android/os/Looper", "myLooper", "()Landroid/os/Looper;");
        const QJniObject main =
            QJniObject::callStaticObjectMethod("android/os/Looper", "getMainLooper", "()Landroid/os/Looper;");
        return QVariant(mine.isValid() && mine == main);
    };
    CHECK(!onMainThread().toBool()); // main() runs on a thread of its own
    QFuture<QVariant> result = QAndroidApplication::runOnAndroidMainThread(std::function<QVariant()>(onMainThread));
    result.waitForFinished();
    CHECK(result.result().toBool());
    bool ran = false;
    QAndroidApplication::runOnAndroidMainThread(std::function<void()>([&ran] { ran = true; })).waitForFinished();
    CHECK(ran);
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testApplication();
    testStringsAndMethods();
    testFields();
    testFailures();
    testThreads();
    qInfo("JNI tests: %s", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
