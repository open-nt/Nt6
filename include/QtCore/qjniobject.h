// MIT License - see LICENSE
#pragma once

#include <QtCore/qjnienvironment.h>

#ifdef __ANDROID__
#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <cstdarg>
#include <memory>
#include <string>
#include <type_traits>

class QJniObject;

namespace QtPrivate {
namespace Jni {

// The JNI signature of a C++ type that can be passed to or returned by Java without being told its signature.
template <typename T, typename = void> struct Type {
    static constexpr bool known = false;
};
#define NT_JNI_TYPE(CppType, Signature)                                                                                \
    template <> struct Type<CppType> {                                                                                 \
        static constexpr bool known = true;                                                                            \
        static constexpr const char *signature = Signature;                                                            \
    };
NT_JNI_TYPE(void, "V")
NT_JNI_TYPE(bool, "Z")
NT_JNI_TYPE(jboolean, "Z")
NT_JNI_TYPE(jbyte, "B")
NT_JNI_TYPE(jchar, "C")
NT_JNI_TYPE(jshort, "S")
NT_JNI_TYPE(jint, "I")
NT_JNI_TYPE(jlong, "J")
NT_JNI_TYPE(jfloat, "F")
NT_JNI_TYPE(jdouble, "D")
NT_JNI_TYPE(jstring, "Ljava/lang/String;")
NT_JNI_TYPE(jobject, "Ljava/lang/Object;")
NT_JNI_TYPE(jclass, "Ljava/lang/Class;")
NT_JNI_TYPE(QString, "Ljava/lang/String;")
#undef NT_JNI_TYPE

template <typename... Args> constexpr bool allKnown = (Type<std::decay_t<Args>>::known && ...);

// What a call returns: Java objects come back as QJniObject.
template <typename Ret> using Result = std::conditional_t<std::is_pointer_v<Ret>, QJniObject, Ret>;

} // namespace Jni
} // namespace QtPrivate

// A Java object, or null, held with a global reference. A call that fails (no such class or method, or a Java
// exception) is reported in the log, the exception is cleared, and the result is null or zero.
class QJniObject {
public:
    QJniObject();
    // A new object of the class, made with its constructor without arguments.
    explicit QJniObject(const char *className);
    // A new object, made with the constructor that has the signature.
    QJniObject(const char *className, const char *signature, ...);
    explicit QJniObject(jclass clazz);
    QJniObject(jclass clazz, const char *signature, ...);
    // Another reference to \a object, which may be local or global; the caller keeps its own reference.
    QJniObject(jobject object); // NOLINT(google-explicit-constructor): as in Qt

    // Takes a local reference over, and deletes it.
    static QJniObject fromLocalRef(jobject localRef);
    static QJniObject fromString(const QString &string);
    QString toString() const;

    bool isValid() const { return object() != nullptr; }
    template <typename T = jobject> T object() const { return static_cast<T>(javaObject()); }
    jclass objectClass() const;
    QByteArray className() const;
    static bool isClassAvailable(const char *className);

    // Methods of the object. With a signature, the arguments are passed as to a JNI call.
    template <typename Ret = void>
    QtPrivate::Jni::Result<Ret> callMethod(const char *methodName, const char *signature, ...) const;
    // Without one, the signature is made from the types of the arguments (primitive types, jstring, jobject and QString).
    template <typename Ret = void, typename... Args, std::enable_if_t<QtPrivate::Jni::allKnown<Args...>, int> = 0>
    QtPrivate::Jni::Result<Ret> callMethod(const char *methodName, const Args &...args) const;
    template <typename Ret = jobject> QJniObject callObjectMethod(const char *methodName) const;
    QJniObject callObjectMethod(const char *methodName, const char *signature, ...) const;

    template <typename Ret = void>
    static QtPrivate::Jni::Result<Ret> callStaticMethod(const char *className, const char *methodName,
                                                        const char *signature, ...);
    template <typename Ret = void>
    static QtPrivate::Jni::Result<Ret> callStaticMethod(jclass clazz, const char *methodName, const char *signature,
                                                        ...);
    template <typename Ret = void, typename... Args, std::enable_if_t<QtPrivate::Jni::allKnown<Args...>, int> = 0>
    static QtPrivate::Jni::Result<Ret> callStaticMethod(const char *className, const char *methodName,
                                                        const Args &...args);
    static QJniObject callStaticObjectMethod(const char *className, const char *methodName, const char *signature, ...);
    static QJniObject callStaticObjectMethod(jclass clazz, const char *methodName, const char *signature, ...);

    // Fields. The type is primitive, or jstring or QString; for other objects give the signature.
    template <typename T> QtPrivate::Jni::Result<T> getField(const char *fieldName) const;
    QJniObject getObjectField(const char *fieldName, const char *signature) const;
    template <typename T> void setField(const char *fieldName, const T &value);
    void setField(const char *fieldName, const char *signature, jobject value);
    template <typename T> static QtPrivate::Jni::Result<T> getStaticField(const char *className, const char *fieldName);
    static QJniObject getStaticObjectField(const char *className, const char *fieldName, const char *signature);
    // The object field with the signature of T, which is jstring, jobject or jclass.
    template <typename T, std::enable_if_t<std::is_pointer_v<T> && QtPrivate::Jni::Type<T>::known, int> = 0>
    static QJniObject getStaticObjectField(const char *className, const char *fieldName) {
        return getStaticObjectField(className, fieldName, QtPrivate::Jni::Type<T>::signature);
    }
    template <typename T> static void setStaticField(const char *className, const char *fieldName, const T &value);

    template <typename T> QJniObject &operator=(T object) {
        *this = QJniObject(static_cast<jobject>(object));
        return *this;
    }

    friend bool operator==(const QJniObject &a, const QJniObject &b) { return a.isSameObject(b.javaObject()); }
    friend bool operator!=(const QJniObject &a, const QJniObject &b) { return !(a == b); }

private:
    struct Data;
    std::shared_ptr<Data> d;
    jobject javaObject() const;
    bool isSameObject(jobject other) const;

    // The parts of the templates that do not depend on the types, in the library. \a kind is the first letter of the
    // return or field type; an object result is returned as a new local reference in the l member.
    static jvalue invoke(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                         va_list args);
    static jvalue invokeA(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                          const jvalue *args);
    static jvalue readField(jobject object, jclass clazz, const char *name, const char *signature, char kind);
    static void writeField(jobject object, jclass clazz, const char *name, const char *signature, char kind,
                           jvalue value);
    static jclass classNamed(const char *className);
    static jstring newString(const QString &string);
    static void deleteLocal(jobject reference);

    template <typename Ret> static QtPrivate::Jni::Result<Ret> convert(const jvalue &value) {
        if constexpr (std::is_same_v<Ret, bool> || std::is_same_v<Ret, jboolean>)
            return static_cast<Ret>(value.z);
        else if constexpr (std::is_same_v<Ret, jbyte>)
            return value.b;
        else if constexpr (std::is_same_v<Ret, jchar>)
            return value.c;
        else if constexpr (std::is_same_v<Ret, jshort>)
            return value.s;
        else if constexpr (std::is_same_v<Ret, jint>)
            return value.i;
        else if constexpr (std::is_same_v<Ret, jlong>)
            return value.j;
        else if constexpr (std::is_same_v<Ret, jfloat>)
            return value.f;
        else if constexpr (std::is_same_v<Ret, jdouble>)
            return value.d;
        else if constexpr (std::is_same_v<Ret, QString>)
            return fromLocalRef(value.l).toString();
        else
            return fromLocalRef(value.l);
    }
    template <typename Ret> static constexpr char kindOf() {
        if constexpr (std::is_pointer_v<Ret> || std::is_same_v<Ret, QString>)
            return 'L';
        else
            return QtPrivate::Jni::Type<Ret>::signature[0];
    }

    // One argument of a call without a signature. A QString becomes a Java string that lives as long as the call.
    struct Argument {
        jvalue value{};
        jobject owned = nullptr;
        template <typename T> void set(const T &v) {
            using U = std::decay_t<T>;
            if constexpr (std::is_same_v<U, QString>) {
                owned = newString(v);
                value.l = owned;
            } else if constexpr (std::is_same_v<U, bool> || std::is_same_v<U, jboolean>) {
                value.z = static_cast<jboolean>(v);
            } else if constexpr (std::is_same_v<U, jbyte>) {
                value.b = v;
            } else if constexpr (std::is_same_v<U, jchar>) {
                value.c = v;
            } else if constexpr (std::is_same_v<U, jshort>) {
                value.s = v;
            } else if constexpr (std::is_same_v<U, jint>) {
                value.i = v;
            } else if constexpr (std::is_same_v<U, jlong>) {
                value.j = v;
            } else if constexpr (std::is_same_v<U, jfloat>) {
                value.f = v;
            } else if constexpr (std::is_same_v<U, jdouble>) {
                value.d = v;
            } else {
                value.l = v;
            }
        }
        ~Argument() {
            if (owned)
                deleteLocal(owned);
        }
    };
    template <typename Ret, typename... Args>
    static QtPrivate::Jni::Result<Ret> invokeDeduced(jobject object, jclass clazz, const char *name,
                                                     const Args &...args) {
        std::string signature = "(";
        ((signature += QtPrivate::Jni::Type<std::decay_t<Args>>::signature), ...);
        signature += ')';
        signature += QtPrivate::Jni::Type<Ret>::signature;
        Argument holders[sizeof...(Args) + 1];
        jvalue values[sizeof...(Args) + 1] = {};
        std::size_t i = 0;
        ((holders[i].set(args), values[i] = holders[i].value, ++i), ...);
        const jvalue result = invokeA(object, clazz, name, signature.c_str(), kindOf<Ret>(), values);
        if constexpr (!std::is_void_v<Ret>)
            return convert<Ret>(result);
    }
};

template <typename Ret>
QtPrivate::Jni::Result<Ret> QJniObject::callMethod(const char *methodName, const char *signature, ...) const {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(javaObject(), nullptr, methodName, signature, kindOf<Ret>(), args);
    va_end(args);
    if constexpr (!std::is_void_v<Ret>)
        return convert<Ret>(result);
}

template <typename Ret, typename... Args, std::enable_if_t<QtPrivate::Jni::allKnown<Args...>, int>>
QtPrivate::Jni::Result<Ret> QJniObject::callMethod(const char *methodName, const Args &...args) const {
    return invokeDeduced<Ret>(javaObject(), nullptr, methodName, args...);
}

template <typename Ret> QJniObject QJniObject::callObjectMethod(const char *methodName) const {
    return invokeDeduced<Ret>(javaObject(), nullptr, methodName);
}

template <typename Ret>
QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(const char *className, const char *methodName,
                                                         const char *signature, ...) {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(nullptr, classNamed(className), methodName, signature, kindOf<Ret>(), args);
    va_end(args);
    if constexpr (!std::is_void_v<Ret>)
        return convert<Ret>(result);
}

template <typename Ret>
QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(jclass clazz, const char *methodName, const char *signature,
                                                         ...) {
    va_list args;
    va_start(args, signature);
    const jvalue result = invoke(nullptr, clazz, methodName, signature, kindOf<Ret>(), args);
    va_end(args);
    if constexpr (!std::is_void_v<Ret>)
        return convert<Ret>(result);
}

template <typename Ret, typename... Args, std::enable_if_t<QtPrivate::Jni::allKnown<Args...>, int>>
QtPrivate::Jni::Result<Ret> QJniObject::callStaticMethod(const char *className, const char *methodName,
                                                         const Args &...args) {
    return invokeDeduced<Ret>(nullptr, classNamed(className), methodName, args...);
}

template <typename T> QtPrivate::Jni::Result<T> QJniObject::getField(const char *fieldName) const {
    return convert<T>(readField(javaObject(), nullptr, fieldName, QtPrivate::Jni::Type<T>::signature, kindOf<T>()));
}

template <typename T> void QJniObject::setField(const char *fieldName, const T &value) {
    Argument argument;
    argument.set(value);
    writeField(javaObject(), nullptr, fieldName, QtPrivate::Jni::Type<std::decay_t<T>>::signature,
               kindOf<std::decay_t<T>>(), argument.value);
}

template <typename T>
QtPrivate::Jni::Result<T> QJniObject::getStaticField(const char *className, const char *fieldName) {
    return convert<T>(
        readField(nullptr, classNamed(className), fieldName, QtPrivate::Jni::Type<T>::signature, kindOf<T>()));
}

template <typename T> void QJniObject::setStaticField(const char *className, const char *fieldName, const T &value) {
    Argument argument;
    argument.set(value);
    writeField(nullptr, classNamed(className), fieldName, QtPrivate::Jni::Type<std::decay_t<T>>::signature,
               kindOf<std::decay_t<T>>(), argument.value);
}
#endif // __ANDROID__
