// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#ifdef __ANDROID__
#include <jni.h>

#include <initializer_list>

// The JNI environment of the calling thread. Nt attaches a thread to the Java VM the first time it is used and
// detaches it when the thread ends.
class QJniEnvironment {
public:
    enum class OutputMode { Silent, Verbose };

    QJniEnvironment();
    // Clears an exception that is still pending, with its description in the log.
    ~QJniEnvironment();
    QJniEnvironment(const QJniEnvironment &) = delete;
    QJniEnvironment &operator=(const QJniEnvironment &) = delete;

    bool isValid() const { return m_env != nullptr; }
    JNIEnv *operator->() const { return m_env; }
    JNIEnv &operator*() const { return *m_env; }
    JNIEnv *jniEnv() const { return m_env; }
    static JavaVM *javaVM();

    jclass findClass(const char *className);
    template <typename T> jclass findClass(const char *className) { return findClass(className); }
    jmethodID findMethod(jclass clazz, const char *methodName, const char *signature);
    jmethodID findStaticMethod(jclass clazz, const char *methodName, const char *signature);
    jfieldID findField(jclass clazz, const char *fieldName, const char *signature);
    jfieldID findStaticField(jclass clazz, const char *fieldName, const char *signature);
    bool registerNativeMethods(const char *className, const JNINativeMethod *methods, int size);
    bool registerNativeMethods(jclass clazz, const JNINativeMethod *methods, int size);
    bool registerNativeMethods(const char *className, std::initializer_list<JNINativeMethod> methods) {
        return registerNativeMethods(className, methods.begin(), static_cast<int>(methods.size()));
    }
    bool checkAndClearExceptions(OutputMode mode = OutputMode::Verbose);
    static bool checkAndClearExceptions(JNIEnv *env, OutputMode mode = OutputMode::Verbose);

private:
    JNIEnv *m_env = nullptr;
};
#endif // __ANDROID__
