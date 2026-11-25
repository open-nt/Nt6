// MIT License - see LICENSE
#include "qjni_android_p.h"

#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#include <QtCore/qlogging.h>
#include <QtCore/qpromise.h>

#include <android/looper.h>

#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <fcntl.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace {

struct JniState {
    JavaVM *vm = nullptr;
    jobject activity = nullptr;    // global reference
    jobject classLoader = nullptr; // global reference to the loader of the activity's class
    jmethodID loadClass = nullptr;
    int sdkVersion = 0;
    pthread_key_t attachedKey{};
    std::mutex classMutex;
    std::map<std::string, jclass> classes; // global references, kept for the process
    // The UI thread of the activity, and what waits to run there.
    ALooper *uiLooper = nullptr;
    int uiPipe[2] = {-1, -1};
    std::mutex uiMutex;
    std::deque<std::function<void()>> uiQueue;
};

JniState &state() {
    static JniState *s = new JniState; // never destroyed: threads may end after the static objects
    return *s;
}

void detachThread(void *) {
    if (state().vm)
        state().vm->DetachCurrentThread();
}

// The environment of this thread, attaching it if Java does not know it.
JNIEnv *environment() {
    JavaVM *vm = state().vm;
    if (!vm)
        return nullptr;
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK)
        return env;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        return nullptr;
    pthread_setspecific(state().attachedKey, env); // detached when the thread ends
    return env;
}

bool clearException(JNIEnv *env, bool describe) {
    if (!env->ExceptionCheck())
        return false;
    if (describe)
        env->ExceptionDescribe(); // to logcat
    env->ExceptionClear();
    return true;
}

// A class by its name, with slashes or dots. FindClass() on a thread that Java did not start sees only the classes of
// the system, so the classes of the package are loaded with the activity's class loader.
jclass findClassByName(JNIEnv *env, const char *className) {
    if (!env || !className)
        return nullptr;
    std::string slashed = className;
    std::replace(slashed.begin(), slashed.end(), '.', '/');
    JniState &s = state();
    std::lock_guard<std::mutex> lock(s.classMutex);
    const auto it = s.classes.find(slashed);
    if (it != s.classes.end())
        return it->second;
    jclass found = env->FindClass(slashed.c_str());
    if (clearException(env, false))
        found = nullptr;
    if (!found && s.classLoader) {
        std::string dotted = slashed;
        std::replace(dotted.begin(), dotted.end(), '/', '.');
        jstring name = env->NewStringUTF(dotted.c_str());
        found = static_cast<jclass>(env->CallObjectMethod(s.classLoader, s.loadClass, name));
        if (clearException(env, false))
            found = nullptr;
        env->DeleteLocalRef(name);
    }
    if (!found) {
        qWarning("QJniEnvironment: the Java class %s was not found", className);
        return nullptr;
    }
    auto *global = static_cast<jclass>(env->NewGlobalRef(found));
    env->DeleteLocalRef(found);
    s.classes.emplace(slashed, global);
    return global;
}

int runUiQueue(int fd, int, void *) {
    char buffer[64];
    while (::read(fd, buffer, sizeof buffer) > 0) {
    }
    JniState &s = state();
    for (;;) {
        std::function<void()> next;
        {
            std::lock_guard<std::mutex> lock(s.uiMutex);
            if (s.uiQueue.empty())
                break;
            next = std::move(s.uiQueue.front());
            s.uiQueue.pop_front();
        }
        next();
    }
    return 1;
}

void postToUiThread(std::function<void()> function) {
    JniState &s = state();
    if (s.uiLooper && ALooper_forThread() == s.uiLooper) {
        function();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s.uiMutex);
        s.uiQueue.push_back(std::move(function));
    }
    const char byte = 1;
    if (::write(s.uiPipe[1], &byte, 1) < 0) {
        // A full pipe: the UI thread has not read the earlier bytes yet, and will run the whole queue.
    }
}

} // namespace

void QtAndroidPrivate::initJni(JavaVM *vm, JNIEnv *env, jobject activity, int sdkVersion) {
    JniState &s = state();
    s.vm = vm;
    s.sdkVersion = sdkVersion;
    pthread_key_create(&s.attachedKey, detachThread);
    s.activity = env->NewGlobalRef(activity);
    jclass activityClass = env->GetObjectClass(activity);
    jclass classClass = env->FindClass("java/lang/Class");
    jmethodID getLoader = env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(activityClass, getLoader);
    if (!clearException(env, true) && loader) {
        s.classLoader = env->NewGlobalRef(loader);
        jclass loaderClass = env->FindClass("java/lang/ClassLoader");
        s.loadClass = env->GetMethodID(loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        env->DeleteLocalRef(loaderClass);
        env->DeleteLocalRef(loader);
    }
    env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(activityClass);
    // The calling thread is the UI thread; its looper runs what runOnAndroidMainThread() queues.
    s.uiLooper = ALooper_forThread();
    if (s.uiLooper && ::pipe(s.uiPipe) == 0) {
        for (int fd : s.uiPipe)
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
        ALooper_acquire(s.uiLooper);
        ALooper_addFd(s.uiLooper, s.uiPipe[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, runUiQueue, nullptr);
    }
}

/*!
    \class QJniEnvironment
    \inmodule NtCore
    \since 6.0
    \brief The QJniEnvironment class gives the JNI environment of the calling thread (Android only).

    Nt attaches a thread to the Java VM the first time it is used, and detaches it when the thread ends:

    \code
    QJniEnvironment env;
    jclass build = env.findClass("android/os/Build");
    env.checkAndClearExceptions();
    \endcode

    The class is only there on Android, with the classes of \l QJniObject.

    \differences
    findClass() keeps a global reference for the life of the process. There is no \c{getStackTrace()}.
    \enddifferences

    \sa QJniObject, {Nt on Android}
*/

/*!
    \enum QJniEnvironment::OutputMode

    What checkAndClearExceptions() writes about the exception.

    \value Silent Nothing.
    \value Verbose Its description, in logcat.
*/

/*!
    Constructs the environment of the calling thread, attaching the thread to Java if necessary.
*/
QJniEnvironment::QJniEnvironment() : m_env(environment()) {}

/*!
    Destroys the object, after clearing an exception that is still pending.
*/
QJniEnvironment::~QJniEnvironment() {
    if (m_env)
        clearException(m_env, true);
}

/*!
    \fn bool QJniEnvironment::isValid() const

    Returns \c true if there is an environment; there is none in a program that no activity started.
*/

/*!
    \fn JNIEnv *QJniEnvironment::operator->() const

    Returns the \c JNIEnv, to call its functions.
*/

/*!
    \fn JNIEnv &QJniEnvironment::operator*() const

    Returns the \c JNIEnv.
*/

/*!
    \fn JNIEnv *QJniEnvironment::jniEnv() const

    Returns the \c JNIEnv, or null.
*/

/*!
    Returns the Java VM of the process.
*/
JavaVM *QJniEnvironment::javaVM() { return state().vm; }

/*!
    Returns the class \a className, such as \c{"android/os/Build"}, or null. Classes of the application's package are
    found from any thread.
*/
jclass QJniEnvironment::findClass(const char *className) { return findClassByName(m_env, className); }

/*!
    Returns the method \a methodName of \a clazz with the \a signature, or null.
*/
jmethodID QJniEnvironment::findMethod(jclass clazz, const char *methodName, const char *signature) {
    if (!m_env || !clazz)
        return nullptr;
    jmethodID id = m_env->GetMethodID(clazz, methodName, signature);
    return clearException(m_env, true) ? nullptr : id;
}

/*!
    Returns the static method \a methodName of \a clazz with the \a signature, or null.
*/
jmethodID QJniEnvironment::findStaticMethod(jclass clazz, const char *methodName, const char *signature) {
    if (!m_env || !clazz)
        return nullptr;
    jmethodID id = m_env->GetStaticMethodID(clazz, methodName, signature);
    return clearException(m_env, true) ? nullptr : id;
}

/*!
    Returns the field \a fieldName of \a clazz with the \a signature, or null.
*/
jfieldID QJniEnvironment::findField(jclass clazz, const char *fieldName, const char *signature) {
    if (!m_env || !clazz)
        return nullptr;
    jfieldID id = m_env->GetFieldID(clazz, fieldName, signature);
    return clearException(m_env, true) ? nullptr : id;
}

/*!
    Returns the static field \a fieldName of \a clazz with the \a signature, or null.
*/
jfieldID QJniEnvironment::findStaticField(jclass clazz, const char *fieldName, const char *signature) {
    if (!m_env || !clazz)
        return nullptr;
    jfieldID id = m_env->GetStaticFieldID(clazz, fieldName, signature);
    return clearException(m_env, true) ? nullptr : id;
}

/*!
    Registers the \a size native \a methods of the class \a className, and returns \c true if it succeeded.
*/
bool QJniEnvironment::registerNativeMethods(const char *className, const JNINativeMethod *methods, int size) {
    return registerNativeMethods(findClass(className), methods, size);
}

/*!
    \overload

    Registers the \a size native \a methods of \a clazz.
*/
bool QJniEnvironment::registerNativeMethods(jclass clazz, const JNINativeMethod *methods, int size) {
    if (!m_env || !clazz)
        return false;
    const bool ok = m_env->RegisterNatives(clazz, methods, size) == JNI_OK;
    return !clearException(m_env, true) && ok;
}

/*!
    \fn bool QJniEnvironment::registerNativeMethods(const char *className, std::initializer_list<JNINativeMethod> methods)
    \overload

    Registers the \a methods of the class \a className.
*/

/*!
    Clears a pending Java exception and returns \c true if there was one; \a mode says whether to log it.
*/
bool QJniEnvironment::checkAndClearExceptions(OutputMode mode) {
    return m_env && clearException(m_env, mode == OutputMode::Verbose);
}

/*!
    \overload

    Does the same for \a env, with \a mode.
*/
bool QJniEnvironment::checkAndClearExceptions(JNIEnv *env, OutputMode mode) {
    return env && clearException(env, mode == OutputMode::Verbose);
}

struct QJniObject::Data {
    jobject object = nullptr; // global reference
    jclass clazz = nullptr;   // global reference, made when first asked for
    Data() = default;
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;
    ~Data() {
        if (JNIEnv *env = environment()) {
            if (object)
                env->DeleteGlobalRef(object);
            if (clazz)
                env->DeleteGlobalRef(clazz);
        }
    }
};

/*!
    \class QJniObject
    \inmodule NtCore
    \since 6.0
    \brief The QJniObject class holds a Java object and calls its methods (Android only).

    A QJniObject keeps a global reference to a Java object. It calls methods and reads fields without the
    boilerplate of JNI; the signature of a call can be given as in JNI, or left out when the arguments and the result
    are primitive types, \c jstring or QString:

    \code
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject name = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    const jint sdk = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
    const QString upper = QJniObject::fromString(u"nt"_s).callMethod<QString>("toUpperCase");
    \endcode

    When something fails (no such class or method, or a Java exception), the exception is logged and cleared, and the
    result is an invalid object or zero.

    \differences
    Calls without a signature take primitive types, \c jstring, \c jobject and QString only: a QJniObject argument
    needs a signature, with its object() as the argument. There is no \c{QtJniTypes}, no \c Q_DECLARE_JNI_CLASS and no
    array helpers.
    \enddifferences

    \sa QJniEnvironment, {Nt on Android}
*/

/*!
    Constructs an invalid object.
*/
QJniObject::QJniObject() : d(std::make_shared<Data>()) {}

/*!
    Constructs a new object of the class \a className with its constructor without arguments.
*/
QJniObject::QJniObject(const char *className) : QJniObject(className, "()V") {}

/*!
    \overload

    Constructs a new object of \a className with the constructor of the \a signature and the arguments that follow.
*/
QJniObject::QJniObject(const char *className, const char *signature, ...) : QJniObject() {
    JNIEnv *env = environment();
    jclass clazz = findClassByName(env, className);
    if (!clazz)
        return;
    jmethodID constructor = env->GetMethodID(clazz, "<init>", signature);
    if (clearException(env, true) || !constructor)
        return;
    va_list args;
    va_start(args, signature);
    jobject local = env->NewObjectV(clazz, constructor, args);
    va_end(args);
    if (!clearException(env, true) && local)
        *this = fromLocalRef(local);
}

/*!
    \overload

    Constructs a new object of \a clazz with its constructor without arguments.
*/
QJniObject::QJniObject(jclass clazz) : QJniObject(clazz, "()V") {}

/*!
    \overload

    Constructs a new object of \a clazz with the constructor of the \a signature and the arguments that follow.
*/
QJniObject::QJniObject(jclass clazz, const char *signature, ...) : QJniObject() {
    JNIEnv *env = environment();
    if (!env || !clazz)
        return;
    jmethodID constructor = env->GetMethodID(clazz, "<init>", signature);
    if (clearException(env, true) || !constructor)
        return;
    va_list args;
    va_start(args, signature);
    jobject local = env->NewObjectV(clazz, constructor, args);
    va_end(args);
    if (!clearException(env, true) && local)
        *this = fromLocalRef(local);
}

/*!
    \overload

    Constructs a reference to \a object, which may be a local or a global reference; the caller's is not touched.
*/
QJniObject::QJniObject(jobject object) : QJniObject() {
    JNIEnv *env = environment();
    if (env && object)
        d->object = env->NewGlobalRef(object);
}

/*!
    Returns an object for the local reference \a localRef, which it deletes.
*/
QJniObject QJniObject::fromLocalRef(jobject localRef) {
    QJniObject result(localRef);
    deleteLocal(localRef);
    return result;
}

/*!
    Returns a Java string with the text of \a string.
*/
QJniObject QJniObject::fromString(const QString &string) { return fromLocalRef(newString(string)); }

/*!
    Returns the text of a Java string, or what \c toString() of another object returns.
*/
QString QJniObject::toString() const {
    JNIEnv *env = environment();
    if (!env || !javaObject())
        return QString();
    const QJniObject text = callObjectMethod("toString", "()Ljava/lang/String;");
    auto *string = text.object<jstring>();
    if (!string)
        return QString();
    const jsize length = env->GetStringLength(string);
    const jchar *chars = env->GetStringChars(string, nullptr);
    if (!chars)
        return QString();
    const QString result(reinterpret_cast<const QChar *>(chars), length);
    env->ReleaseStringChars(string, chars);
    return result;
}

/*!
    \fn bool QJniObject::isValid() const

    Returns \c true if the object is not null.
*/

/*!
    \fn template <typename T> T QJniObject::object() const

    Returns the global reference, as a \c jobject or another JNI type. It belongs to this QJniObject.
*/

jobject QJniObject::javaObject() const { return d->object; }

/*!
    Returns the class of the object, or null. The reference belongs to this object.
*/
jclass QJniObject::objectClass() const {
    JNIEnv *env = environment();
    if (!env || !d->object)
        return nullptr;
    if (!d->clazz) {
        jclass local = env->GetObjectClass(d->object);
        d->clazz = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
    }
    return d->clazz;
}

/*!
    Returns the name of the object's class, with slashes, such as \c{java/lang/String}.
*/
QByteArray QJniObject::className() const {
    const QJniObject clazz(static_cast<jobject>(objectClass()));
    QByteArray name = clazz.callObjectMethod("getName", "()Ljava/lang/String;").toString().toUtf8();
    name.replace('.', '/');
    return name;
}

/*!
    Returns \c true if the class \a className can be loaded.
*/
bool QJniObject::isClassAvailable(const char *className) { return classNamed(className) != nullptr; }

jclass QJniObject::classNamed(const char *className) { return findClassByName(environment(), className); }

jstring QJniObject::newString(const QString &string) {
    JNIEnv *env = environment();
    if (!env)
        return nullptr;
    return env->NewString(reinterpret_cast<const jchar *>(string.constData()), static_cast<jsize>(string.size()));
}

void QJniObject::deleteLocal(jobject reference) {
    JNIEnv *env = environment();
    if (env && reference)
        env->DeleteLocalRef(reference);
}

bool QJniObject::isSameObject(jobject other) const {
    JNIEnv *env = environment();
    return env ? env->IsSameObject(d->object, other) == JNI_TRUE : d->object == other;
}

#define NT_JNI_CALL_ALL                                                                                                \
    NT_JNI_CALL('Z', z, Boolean)                                                                                       \
    NT_JNI_CALL('B', b, Byte)                                                                                          \
    NT_JNI_CALL('C', c, Char)                                                                                          \
    NT_JNI_CALL('S', s, Short)                                                                                         \
    NT_JNI_CALL('I', i, Int)                                                                                           \
    NT_JNI_CALL('J', j, Long)                                                                                          \
    NT_JNI_CALL('F', f, Float)                                                                                         \
    NT_JNI_CALL('D', d, Double)                                                                                        \
    NT_JNI_CALL('L', l, Object)

// The method of \a object, or the static method of \a clazz, with its class; null when it cannot be called.
static jmethodID lookupMethod(JNIEnv *env, jobject object, jclass clazz, const char *name, const char *signature) {
    if (!env || (!object && !clazz))
        return nullptr;
    jclass owner = object ? env->GetObjectClass(object) : clazz;
    jmethodID method =
        object ? env->GetMethodID(owner, name, signature) : env->GetStaticMethodID(owner, name, signature);
    if (object)
        env->DeleteLocalRef(owner);
    return clearException(env, true) ? nullptr : method;
}

jvalue QJniObject::invoke(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                          va_list args) {
    jvalue result{};
    JNIEnv *env = environment();
    jmethodID method = lookupMethod(env, object, clazz, name, signature);
    if (!method)
        return result;
    if (kind == 'V') {
        if (object)
            env->CallVoidMethodV(object, method, args);
        else
            env->CallStaticVoidMethodV(clazz, method, args);
    } else {
        switch (kind) {
#undef NT_JNI_CALL
#define NT_JNI_CALL(Kind, Member, Name)                                                                                \
    case Kind:                                                                                                         \
        result.Member = object ? env->Call##Name##MethodV(object, method, args)                                        \
                               : env->CallStatic##Name##MethodV(clazz, method, args);                                  \
        break;
            NT_JNI_CALL_ALL
        default:
            break;
        }
    }
    if (clearException(env, true))
        result = jvalue{};
    return result;
}

jvalue QJniObject::invokeA(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                           const jvalue *args) {
    jvalue result{};
    JNIEnv *env = environment();
    jmethodID method = lookupMethod(env, object, clazz, name, signature);
    if (!method)
        return result;
    if (kind == 'V') {
        if (object)
            env->CallVoidMethodA(object, method, args);
        else
            env->CallStaticVoidMethodA(clazz, method, args);
    } else {
        switch (kind) {
#undef NT_JNI_CALL
#define NT_JNI_CALL(Kind, Member, Name)                                                                                \
    case Kind:                                                                                                         \
        result.Member = object ? env->Call##Name##MethodA(object, method, args)                                        \
                               : env->CallStatic##Name##MethodA(clazz, method, args);                                  \
        break;
            NT_JNI_CALL_ALL
        default:
            break;
        }
    }
    if (clearException(env, true))
        result = jvalue{};
    return result;
}

jvalue QJniObject::readField(jobject object, jclass clazz, const char *name, const char *signature, char kind) {
    jvalue result{};
    JNIEnv *env = environment();
    if (!env || (!object && !clazz))
        return result;
    jclass owner = object ? env->GetObjectClass(object) : clazz;
    jfieldID field = object ? env->GetFieldID(owner, name, signature) : env->GetStaticFieldID(owner, name, signature);
    if (object)
        env->DeleteLocalRef(owner);
    if (clearException(env, true) || !field)
        return result;
    switch (kind) {
#undef NT_JNI_CALL
#define NT_JNI_CALL(Kind, Member, Name)                                                                                \
    case Kind:                                                                                                         \
        result.Member = object ? env->Get##Name##Field(object, field) : env->GetStatic##Name##Field(clazz, field);     \
        break;
        NT_JNI_CALL_ALL
    default:
        break;
    }
    if (clearException(env, true))
        result = jvalue{};
    return result;
}

void QJniObject::writeField(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                            jvalue value) {
    JNIEnv *env = environment();
    if (!env || (!object && !clazz))
        return;
    jclass owner = object ? env->GetObjectClass(object) : clazz;
    jfieldID field = object ? env->GetFieldID(owner, name, signature) : env->GetStaticFieldID(owner, name, signature);
    if (object)
        env->DeleteLocalRef(owner);
    if (clearException(env, true) || !field)
        return;
    switch (kind) {
#undef NT_JNI_CALL
#define NT_JNI_CALL(Kind, Member, Name)                                                                                \
    case Kind:                                                                                                         \
        if (object)                                                                                                    \
            env->Set##Name##Field(object, field, value.Member);                                                        \
        else                                                                                                           \
            env->SetStatic##Name##Field(clazz, field, value.Member);                                                   \
        break;
        NT_JNI_CALL_ALL
    default:
        break;
    }
    clearException(env, true);
}
#undef NT_JNI_CALL
#undef NT_JNI_CALL_ALL

/*!
    \fn template <typename Ret> QtPrivate::Jni::Result<Ret> QJniObject::callMethod(const char *methodName, const char *signature, ...) const

    Calls the method \a methodName of the \a signature with the arguments that follow, and returns the result. Java
    objects come back as QJniObject, and \c{callMethod<QString>()} as a QString.
*/

/*!
    \fn template <typename Ret, typename... Args> QtPrivate::Jni::Result<Ret> QJniObject::callMethod(const char *methodName, const Args &...args) const
    \overload

    Calls the method \a methodName with \a args, making the signature from their types and from \c Ret.
*/

/*!
    \fn template <typename Ret> QJniObject QJniObject::callObjectMethod(const char *methodName) const

    Calls the method \a methodName without arguments that returns \c Ret.
*/

/*!
    Calls the method \a methodName of the \a signature, which returns an object.
*/
QJniObject QJniObject::callObjectMethod(const char *methodName, const char *signature, ...) const {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(javaObject(), nullptr, methodName, signature, 'L', args);
    va_end(args);
    return fromLocalRef(result.l);
}

/*!
    \fn template <typename Ret> QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(const char *className, const char *methodName, const char *signature, ...)

    Calls the static method \a methodName of the class \a className with the \a signature.
*/

/*!
    \fn template <typename Ret> QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(jclass clazz, const char *methodName, const char *signature, ...)
    \overload

    Calls the static method \a methodName of \a clazz with the \a signature.
*/

/*!
    \fn template <typename Ret, typename... Args> QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(const char *className, const char *methodName, const Args &...args)
    \overload

    Calls the static method \a methodName of \a className with \a args.
*/

/*!
    Calls the static method \a methodName of the class \a className with the \a signature, which returns an object.
*/
QJniObject QJniObject::callStaticObjectMethod(const char *className, const char *methodName, const char *signature,
                                              ...) {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(nullptr, classNamed(className), methodName, signature, 'L', args);
    va_end(args);
    return fromLocalRef(result.l);
}

/*!
    \overload

    Calls the static method \a methodName of \a clazz with the \a signature, which returns an object.
*/
QJniObject QJniObject::callStaticObjectMethod(jclass clazz, const char *methodName, const char *signature, ...) {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(nullptr, clazz, methodName, signature, 'L', args);
    va_end(args);
    return fromLocalRef(result.l);
}

/*!
    \fn template <typename T> QtPrivate::Jni::Result<T> QJniObject::getField(const char *fieldName) const

    Returns the field \a fieldName.
*/

/*!
    Returns the object in the field \a fieldName, which has the \a signature.
*/
QJniObject QJniObject::getObjectField(const char *fieldName, const char *signature) const {
    return fromLocalRef(readField(javaObject(), nullptr, fieldName, signature, 'L').l);
}

/*!
    \fn template <typename T> void QJniObject::setField(const char *fieldName, const T &value)

    Sets the field \a fieldName to \a value.
*/

/*!
    \overload

    Sets the field \a fieldName, which has the \a signature, to the object \a value.
*/
void QJniObject::setField(const char *fieldName, const char *signature, jobject value) {
    jvalue v{};
    v.l = value;
    writeField(javaObject(), nullptr, fieldName, signature, 'L', v);
}

/*!
    \fn template <typename T> QtPrivate::Jni::Result<T> QJniObject::getStaticField(const char *className, const char *fieldName)

    Returns the static field \a fieldName of the class \a className.
*/

/*!
    Returns the object in the static field \a fieldName of \a className, which has the \a signature.
*/
QJniObject QJniObject::getStaticObjectField(const char *className, const char *fieldName, const char *signature) {
    return fromLocalRef(readField(nullptr, classNamed(className), fieldName, signature, 'L').l);
}

/*!
    \fn template <typename T> void QJniObject::setStaticField(const char *className, const char *fieldName, const T &value)

    Sets the static field \a fieldName of \a className to \a value.
*/

/*!
    \fn template <typename T> QJniObject &QJniObject::operator=(T object)

    Refers to the JNI reference \a object instead.
*/

/*!
    \class QNativeInterface::QAndroidApplication
    \inmodule NtCore
    \since 6.0
    \brief The QAndroidApplication struct gives access to the Android application (Android only).

    \code
    using QNativeInterface::QAndroidApplication;
    QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject text = QJniObject::fromString(u"Saved"_s);
        QJniObject toast = QJniObject::callStaticObjectMethod("android/widget/Toast", "makeText",
            "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;",
            QAndroidApplication::context(), text.object<jstring>(), 0);
        toast.callMethod<void>("show");
    });
    \endcode

    \differences
    context() returns a \c jobject. There is no \c hideSplashScreen(), and runOnAndroidMainThread() takes no
    deadline.
    \enddifferences
*/

/*!
    \fn jobject QNativeInterface::QAndroidApplication::context()

    Returns the activity, a global reference that is valid for the life of the process.
*/
jobject QNativeInterface::QAndroidApplication::context() { return state().activity; }

/*!
    \fn bool QNativeInterface::QAndroidApplication::isActivityContext()

    Returns \c true if context() is an activity.
*/
bool QNativeInterface::QAndroidApplication::isActivityContext() { return state().activity != nullptr; }

/*!
    \fn int QNativeInterface::QAndroidApplication::sdkVersion()

    Returns the API level of the device.
*/
int QNativeInterface::QAndroidApplication::sdkVersion() { return state().sdkVersion; }

/*!
    \fn QFuture<QVariant> QNativeInterface::QAndroidApplication::runOnAndroidMainThread(const std::function<QVariant()> &runnable)

    Runs \a runnable on the UI thread and returns a future with its result.
*/
QFuture<QVariant>
QNativeInterface::QAndroidApplication::runOnAndroidMainThread(const std::function<QVariant()> &runnable) {
    auto promise = std::make_shared<QPromise<QVariant>>();
    QFuture<QVariant> future = promise->future();
    promise->start();
    postToUiThread([promise, runnable] {
        promise->addResult(runnable());
        JNIEnv *env = environment();
        if (env)
            clearException(env, true);
        promise->finish();
    });
    return future;
}

/*!
    \fn QFuture<void> QNativeInterface::QAndroidApplication::runOnAndroidMainThread(const std::function<void()> &runnable)
    \overload

    Runs \a runnable, which returns nothing, on the UI thread.
*/
QFuture<void> QNativeInterface::QAndroidApplication::runOnAndroidMainThread(const std::function<void()> &runnable) {
    auto promise = std::make_shared<QPromise<void>>();
    QFuture<void> future = promise->future();
    promise->start();
    postToUiThread([promise, runnable] {
        runnable();
        JNIEnv *env = environment();
        if (env)
            clearException(env, true);
        promise->finish();
    });
    return future;
}
