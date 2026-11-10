// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

class QObject;
class QMetaMethod;
class QMetaEnum;
class QMetaProperty;
class QMetaClassInfo;
struct QMetaObject;

namespace QtPrivate {

struct ConnectionData;

// Identifies a signal by the bytes of its pointer-to-member-function.
struct SignalId {
    unsigned char bytes[32] = {};

    bool operator==(const SignalId &o) const noexcept { return std::memcmp(bytes, o.bytes, sizeof bytes) == 0; }

    template <typename PMF> static SignalId of(PMF pmf) noexcept {
        static_assert(sizeof(PMF) <= sizeof(bytes), "member pointer too large");
        SignalId id;
        std::memcpy(id.bytes, &pmf, sizeof(PMF));
        return id;
    }
};

template <typename F> struct MemberClass;
template <typename R, typename C, typename... A> struct MemberClass<R (C::*)(A...)> {
    using type = C;
};
template <typename R, typename C, typename... A> struct MemberClass<R (C::*)(A...) const> {
    using type = C;
};
template <typename R, typename C, typename... A> struct MemberClass<R (C::*)(A...) noexcept> {
    using type = C;
};
template <typename R, typename C, typename... A> struct MemberClass<R (C::*)(A...) const noexcept> {
    using type = C;
};

// The context object that QMetaObject::invokeMethod() takes for a function of type F: an instance
// of the class for a pointer to a member function, and any QObject for another function.
template <typename F, bool = std::is_member_function_pointer_v<F>> struct InvokeContext {
    using type = QObject;
};
template <typename F> struct InvokeContext<F, true> {
    using type = typename MemberClass<F>::type;
};

// True if QMetaObject::invokeMethod() can call a function of type F with arguments of the types A;
// a member function is called on the context object.
template <typename F, typename... A>
inline constexpr bool CanInvoke = std::is_member_function_pointer_v<std::decay_t<F>>
                                      ? std::is_invocable_v<F, typename InvokeContext<std::decay_t<F>>::type *, A...>
                                      : std::is_invocable_v<F, A...>;

// The result of that call, as the member `type`.
template <typename F, typename... A>
struct InvokeResultOf : std::conditional_t<std::is_member_function_pointer_v<std::decay_t<F>>,
                                           std::invoke_result<F, typename InvokeContext<std::decay_t<F>>::type *, A...>,
                                           std::invoke_result<F, A...>> {};

// True for the calls that the overloads of invokeMethod() with plain arguments handle: a member
// function, or a functor that is given arguments.
template <typename F, typename... A>
inline constexpr bool InvokesWithArguments =
    CanInvoke<F, A...> && (std::is_member_function_pointer_v<std::decay_t<F>> || sizeof...(A) > 0);

// One method as ntmoc describes it. Types are normalized.
struct MetaMethodData {
    const char *name;
    const char *returnType;
    const char *const *paramTypes;
    const char *const *paramNames;
    unsigned char paramCount;
    unsigned char type;   // QMetaMethod::MethodType
    unsigned char access; // QMetaMethod::Access
    unsigned char flags;  // 1: cloned, 2: scriptable
    int revision;
    const char *tag;
    SignalId (*signalId)(); // signals only
};

struct MetaPropertyData {
    const char *name;
    const char *typeName;
    unsigned flags;
    int notifySignal; // local method index, or -1
    int revision;
};

struct MetaEnumKey {
    const char *name;
    int value;
};

struct MetaEnumData {
    const char *name;
    bool isFlag;
    bool isScoped;
    unsigned keyCount;
    const MetaEnumKey *keys;
};

struct MetaClassInfoData {
    const char *name;
    const char *value;
};

struct MetaObjectData {
    const char *className;
    const MetaMethodData *methods;
    unsigned methodCount;
    const MetaMethodData *constructors;
    unsigned constructorCount;
    const MetaPropertyData *properties;
    unsigned propertyCount;
    const MetaEnumData *enums;
    unsigned enumCount;
    const MetaClassInfoData *classInfos;
    unsigned classInfoCount;
};

// Property flag bits in MetaPropertyData::flags.
enum PropertyFlag : unsigned {
    PropReadable = 0x1,
    PropWritable = 0x2,
    PropResettable = 0x4,
    PropConstant = 0x8,
    PropFinal = 0x10,
    PropRequired = 0x20,
    PropMember = 0x40,
    // Two bits each: 0 false, 1 true, 2 asks the object.
    PropDesignableShift = 8,
    PropScriptableShift = 10,
    PropStoredShift = 12,
    PropUserShift = 14
};

} // namespace QtPrivate

/*!
    \class QGenericArgument
    \brief The QGenericArgument class is an internal helper class for marshalling arguments.

    Create one with \c Q_ARG() to pass an argument to QMetaObject::invokeMethod().
    It holds the name of the argument's type and a pointer to its value.

    \differences
    None.
    \enddifferences

    \sa QMetaObject::invokeMethod()
*/
class QGenericArgument {
public:
    /*!
        Constructs a QGenericArgument for a value of the type called \a aName at \a aData.
    */
    constexpr QGenericArgument(const char *aName = nullptr, const void *aData = nullptr) : _data(aData), _name(aName) {}
    /*!
        Returns the address of the value.
    */
    void *data() const { return const_cast<void *>(_data); }
    /*!
        Returns the name of the value's type.
    */
    const char *name() const { return _name; }

private:
    const void *_data;
    const char *_name;
};

/*!
    \class QGenericReturnArgument
    \brief The QGenericReturnArgument class is an internal helper class for marshalling return values.

    Create one with \c Q_RETURN_ARG() to receive the result of QMetaObject::invokeMethod().

    \differences
    None.
    \enddifferences

    \sa QMetaObject::invokeMethod()
*/
class QGenericReturnArgument : public QGenericArgument {
public:
    /*!
        Constructs a QGenericReturnArgument that stores a value of the type called \a aName at \a aData.
    */
    constexpr QGenericReturnArgument(const char *aName = nullptr, void *aData = nullptr)
        : QGenericArgument(aName, aData) {}
};

/*!
    \class QArgument
    \brief The QArgument class is a template helper that holds an argument of type \c T.

    \c Q_ARG(Type, value) creates one.

    \differences
    None.
    \enddifferences

    \sa QGenericArgument
*/
template <class T> class QArgument : public QGenericArgument {
public:
    /*!
        Constructs an argument called \a aName that refers to \a aData.
    */
    QArgument(const char *aName, const T &aData) : QGenericArgument(aName, static_cast<const void *>(&aData)) {}
};

// The specialization of QArgument for a reference to a value.
template <class T> class QArgument<T &> : public QGenericArgument {
public:
    QArgument(const char *aName, T &aData) : QGenericArgument(aName, static_cast<const void *>(&aData)) {}
};

/*!
    \class QReturnArgument
    \brief The QReturnArgument class is a template helper that holds a return value of type \c T.

    \c Q_RETURN_ARG(Type, variable) creates one.

    \differences
    None.
    \enddifferences

    \sa QGenericReturnArgument
*/
template <class T> class QReturnArgument : public QGenericReturnArgument {
public:
    /*!
        Constructs a return value called \a aName that is stored in \a aData.
    */
    QReturnArgument(const char *aName, T &aData) : QGenericReturnArgument(aName, static_cast<void *>(&aData)) {}
};

/*!
    \class QTemplatedMetaMethodReturnArgument
    \brief The QTemplatedMetaMethodReturnArgument class refers to the variable that receives the result of QMetaObject::invokeMethod().
    \since 6.5

    qReturnArg() creates one from a variable of type \c T. Pass it to the
    overloads of QMetaObject::invokeMethod() that take a function and plain
    arguments:

    \code
    int sum = 0;
    QMetaObject::invokeMethod(&calculator, &Calculator::add, qReturnArg(sum), 2, 3);
    \endcode

    \differences
    The class holds only the address of the variable; there is no
    \c QMetaMethodReturnArgument base class with a type name and a meta-type.
    The overloads of QMetaObject::invokeMethod() and QMetaMethod::invoke()
    that take the name of a method do not accept it; use \c Q_RETURN_ARG()
    there.
    \enddifferences

    \sa QMetaObject::invokeMethod()
*/
template <typename T> struct QTemplatedMetaMethodReturnArgument {
    /*!
        The address of the variable that receives the result.
    */
    void *data;
};

// Returns the argument that makes QMetaObject::invokeMethod() store the result of the call in data.
template <typename T> QTemplatedMetaMethodReturnArgument<T> qReturnArg(T &data) {
    return QTemplatedMetaMethodReturnArgument<T>{static_cast<void *>(&data)};
}

class QString;
namespace QtPrivate {
// Translates through the translators installed on QCoreApplication; used by tr().
QString translate(const char *context, const char *sourceText, const char *disambiguation, int n);
} // namespace QtPrivate

// Q_OBJECT and Q_GADGET declare what ntmoc defines. A class that never goes through ntmoc leaves
// them undefined, which is fine until something refers to staticMetaObject.
#define Q_OBJECT                                                                                                       \
public:                                                                                                                \
    static const QMetaObject staticMetaObject;                                                                         \
    static void qt_static_metacall(QObject *, QMetaObject::Call, int, void **);                                        \
    static QString tr(const char *sourceText, const char *disambiguation = nullptr, int n = -1) {                      \
        return QtPrivate::translate(staticMetaObject.className(), sourceText, disambiguation, n);                      \
    }                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
    struct QPrivateSignal {                                                                                            \
        explicit QPrivateSignal() = default;                                                                           \
    };
#define Q_GADGET                                                                                                       \
public:                                                                                                                \
    static const QMetaObject staticMetaObject;                                                                         \
    static void qt_static_metacall(QObject *, QMetaObject::Call, int, void **);                                        \
                                                                                                                       \
private:
#define Q_NAMESPACE extern const QMetaObject staticMetaObject;
#define Q_NAMESPACE_EXPORT(...) Q_NAMESPACE

#define Q_ENUM(ENUM)                                                                                                   \
    friend constexpr const QMetaObject *qt_getEnumMetaObject(ENUM) noexcept { return &staticMetaObject; }              \
    friend constexpr const char *qt_getEnumName(ENUM) noexcept { return #ENUM; }
#define Q_FLAG(ENUM) Q_ENUM(ENUM)
#define Q_ENUM_NS(ENUM)                                                                                                \
    inline constexpr const QMetaObject *qt_getEnumMetaObject(ENUM) noexcept { return &staticMetaObject; }              \
    inline constexpr const char *qt_getEnumName(ENUM) noexcept { return #ENUM; }
#define Q_FLAG_NS(ENUM) Q_ENUM_NS(ENUM)

#define Q_ARG(type, data) QArgument<type>(#type, data)
#define Q_RETURN_ARG(type, data) QReturnArgument<type>(#type, data)

#define SLOT(a) "1" #a
#define SIGNAL(a) "2" #a
#define METHOD(a) "0" #a

/*!
    \class QMetaObject
    \brief The QMetaObject class contains meta-information about Nt objects.

    Every class that declares \c Q_OBJECT or \c Q_GADGET and goes through
    \c ntmoc has a static QMetaObject, \c staticMetaObject, that describes its
    class name, base class, signals, slots, invokable methods, properties,
    enumerators and class information. For a QObject,
    \l{QObject::metaObject()}{metaObject()} returns the meta-object of its
    dynamic type.

    \code
    const QMetaObject *mo = obj->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i)
        qDebug() << mo->property(i).name();
    \endcode

    Methods and properties are numbered across the inheritance chain: the
    base class has the lowest indexes, and methodOffset() and
    propertyOffset() give the first index of this class. Signals come first
    in a class, then slots, then \c Q_INVOKABLE methods, each in declaration
    order. A method with default arguments gets one extra entry for each
    shorter call.

    \differences
    The layout of the public \c d data member is Nt's own and is not
    binary-compatible with Qt, and the code that \c ntmoc generates differs
    from that of Qt's moc. There are no \c qt_metacast() and \c qt_metacall()
    functions, no \c QMetaObject::tr() or \c changeGuard(), and
    \c qobject_cast() stays a wrapper around \c dynamic_cast.
    The invokeMethod() overloads that take the name of a method need
    \c Q_ARG() and \c Q_RETURN_ARG(); plain arguments and qReturnArg() are
    for the overloads that take a function. A class that \c ntmoc did not process has no meta-object of its
    own: QObject::metaObject() returns that of its nearest base class that
    has one.
    \enddifferences

    \sa QMetaMethod, QMetaProperty, QMetaEnum, QMetaClassInfo, QObject, {Meta-Object System}
*/
struct QMetaObject {
    class Connection {
    public:
        /*!
            Constructs an invalid connection handle.
        */
        Connection() = default;
        explicit operator bool() const noexcept;

    private:
        friend class QObject;
        std::weak_ptr<QtPrivate::ConnectionData> d;
    };

    /*!
        \enum QMetaObject::Call

        This enum describes the calls that a meta-object's \c static_metacall() function handles.

        \value InvokeMetaMethod Invoke a method.
        \value ReadProperty Read a property.
        \value WriteProperty Write a property.
        \value ResetProperty Reset a property.
        \value CreateInstance Create an instance with a constructor.
        \value IndexOfMethod Find the index of a method; unused by Nt.
        \value RegisterPropertyMetaType Get the QMetaType of a property.
        \value RegisterMethodArgumentMetaType Get the QMetaType of a method argument.
        \value BindableProperty Get the bindable of a property; unused by Nt.
        \value CustomCall A call that a class defines itself.
        \value QueryPropertyDesignable Ask whether a property is designable.
        \value QueryPropertyScriptable Ask whether a property is scriptable.
        \value QueryPropertyStored Ask whether a property is stored.
        \value QueryPropertyUser Ask whether a property is the user property.
    */
    enum Call {
        InvokeMetaMethod,
        ReadProperty,
        WriteProperty,
        ResetProperty,
        CreateInstance,
        IndexOfMethod,
        RegisterPropertyMetaType,
        RegisterMethodArgumentMetaType,
        BindableProperty,
        CustomCall,
        QueryPropertyDesignable,
        QueryPropertyScriptable,
        QueryPropertyStored,
        QueryPropertyUser
    };

    /*!
        The type of the function that ntmoc generates in each class to invoke methods and read and
        write properties; see \c QMetaObject::d.
    */
    using StaticMetacallFunction = void (*)(QObject *, Call, int, void **);

    const char *className() const;
    const QMetaObject *superClass() const;
    bool inherits(const QMetaObject *metaObject) const noexcept;
    QObject *cast(QObject *obj) const;
    const QObject *cast(const QObject *obj) const;

    int methodOffset() const;
    int enumeratorOffset() const;
    int propertyOffset() const;
    int classInfoOffset() const;

    int constructorCount() const;
    int methodCount() const;
    int enumeratorCount() const;
    int propertyCount() const;
    int classInfoCount() const;

    int indexOfConstructor(const char *constructor) const;
    int indexOfMethod(const char *method) const;
    int indexOfSignal(const char *signal) const;
    int indexOfSlot(const char *slot) const;
    int indexOfEnumerator(const char *name) const;
    int indexOfProperty(const char *name) const;
    int indexOfClassInfo(const char *name) const;

    QMetaMethod constructor(int index) const;
    QMetaMethod method(int index) const;
    QMetaEnum enumerator(int index) const;
    QMetaProperty property(int index) const;
    QMetaClassInfo classInfo(int index) const;
    QMetaProperty userProperty() const;

    static bool checkConnectArgs(const char *signal, const char *method);
    static bool checkConnectArgs(const QMetaMethod &signal, const QMetaMethod &method);
    static QByteArray normalizedSignature(const char *method);
    static QByteArray normalizedType(const char *type);

    static bool connect(const QObject *sender, int signal_index, const QObject *receiver, int method_index,
                        int type = 0, int *types = nullptr);
    static bool disconnect(const QObject *sender, int signal_index, const QObject *receiver, int method_index);
    static void connectSlotsByName(QObject *o);
    static void activate(QObject *sender, int signal_index, void **argv);
    static void activate(QObject *sender, const QMetaObject *mo, int local_signal_index, void **argv);

    static bool invokeMethod(QObject *obj, const char *member, Qt::ConnectionType type, QGenericReturnArgument ret,
                             QGenericArgument val0 = QGenericArgument(nullptr),
                             QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                             QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                             QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                             QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                             QGenericArgument val9 = QGenericArgument());

    /*!
        \overload
        Invokes \a member on \a obj with the connection type \a type, passing the arguments
        \a val0, \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9,
        and storing no return value.
    */
    static bool invokeMethod(QObject *obj, const char *member, Qt::ConnectionType type,
                             QGenericArgument val0 = QGenericArgument(nullptr),
                             QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                             QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                             QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                             QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                             QGenericArgument val9 = QGenericArgument()) {
        return invokeMethod(obj, member, type, QGenericReturnArgument(), val0, val1, val2, val3, val4, val5, val6, val7,
                            val8, val9);
    }

    /*!
        \overload
        Invokes \a member on \a obj with \c Qt::AutoConnection, storing the result in \a ret and
        passing the arguments \a val0, \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7,
        \a val8 and \a val9.
    */
    static bool invokeMethod(QObject *obj, const char *member, QGenericReturnArgument ret,
                             QGenericArgument val0 = QGenericArgument(nullptr),
                             QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                             QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                             QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                             QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                             QGenericArgument val9 = QGenericArgument()) {
        return invokeMethod(obj, member, Qt::AutoConnection, ret, val0, val1, val2, val3, val4, val5, val6, val7, val8,
                            val9);
    }

    /*!
        \overload
        Invokes \a member on \a obj with \c Qt::AutoConnection, passing the arguments \a val0,
        \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9, and
        storing no return value.
    */
    static bool invokeMethod(QObject *obj, const char *member, QGenericArgument val0 = QGenericArgument(nullptr),
                             QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                             QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                             QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                             QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                             QGenericArgument val9 = QGenericArgument()) {
        return invokeMethod(obj, member, Qt::AutoConnection, QGenericReturnArgument(), val0, val1, val2, val3, val4,
                            val5, val6, val7, val8, val9);
    }

    /*!
        \overload
        Calls \a function in the context of \a context. With \c Qt::DirectConnection or
        \c Qt::AutoConnection it runs at once; with \c Qt::QueuedConnection it runs from the
        event loop, and \a ret must then be \c nullptr. If \a ret is not \c nullptr, the
        result is stored there. Returns \c false if \a context is \c nullptr.
    */
    template <typename Functor, typename = std::enable_if_t<std::is_invocable_v<Functor>>>
    static bool invokeMethod(QObject *context, Functor &&function, Qt::ConnectionType type = Qt::AutoConnection,
                             std::remove_cv_t<std::remove_reference_t<std::invoke_result_t<Functor>>> *ret = nullptr) {
        return invokeCallable(context, std::forward<Functor>(function), type, ret);
    }

    /*!
        \overload
        Calls \a function in the context of \a context with \c Qt::AutoConnection and stores
        the result in \a ret.
    */
    template <typename Functor, typename = std::enable_if_t<std::is_invocable_v<Functor>>>
    static bool invokeMethod(QObject *context, Functor &&function,
                             std::remove_cv_t<std::remove_reference_t<std::invoke_result_t<Functor>>> *ret) {
        return invokeMethod(context, std::forward<Functor>(function), Qt::AutoConnection, ret);
    }

    /*!
        \overload
        \since 6.7

        Calls \a function with the arguments \a args in the context of \a context, using the
        connection type \a type, and returns \c true if the call was made or queued.
        \a function is a pointer to a member function of the class of \a context, which is
        called on \a context, or a functor that takes \a args:

        \code
        QMetaObject::invokeMethod(&label, &Label::setText, Qt::QueuedConnection, QString("Done"));
        QMetaObject::invokeMethod(&label, [](int code) { report(code); }, Qt::QueuedConnection, 3);
        \endcode

        A queued call copies \a function and \a args, and runs when control returns to the
        event loop of the thread of \a context; nothing runs if \a context is destroyed first.
        The arguments are checked when the call is compiled, so their types need not be
        registered with QMetaType. Returns \c false if \a context is \c nullptr.
    */
    template <typename Functor, typename... Args,
              std::enable_if_t<QtPrivate::InvokesWithArguments<Functor, Args...>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, Qt::ConnectionType type, Args &&...args) {
        return invokeFunction(context, std::forward<Functor>(function), type,
                              static_cast<InvokeResult<Functor, Args...> *>(nullptr), std::forward<Args>(args)...);
    }

    /*!
        \overload
        \since 6.7

        Calls \a function with the arguments \a args in the context of \a context with
        \c Qt::AutoConnection: at once if \a context lives in the calling thread, and from the
        event loop of its thread otherwise.
    */
    template <typename Functor, typename... Args,
              std::enable_if_t<QtPrivate::InvokesWithArguments<Functor, Args...>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, Args &&...args) {
        return invokeFunction(context, std::forward<Functor>(function), Qt::AutoConnection,
                              static_cast<InvokeResult<Functor, Args...> *>(nullptr), std::forward<Args>(args)...);
    }

    /*!
        \overload
        \since 6.7

        Calls \a function with the arguments \a args in the context of \a context, using the
        connection type \a type, and stores the result in the variable that \a ret refers to;
        create \a ret with qReturnArg(). A queued call cannot return a value, so the function
        returns \c false if the call would be queued.
    */
    template <typename Functor, typename Ret, typename... Args,
              std::enable_if_t<QtPrivate::CanInvoke<Functor, Args...>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, Qt::ConnectionType type, QTemplatedMetaMethodReturnArgument<Ret> ret,
                             Args &&...args) {
        return invokeFunction(context, std::forward<Functor>(function), type, static_cast<Ret *>(ret.data),
                              std::forward<Args>(args)...);
    }

    /*!
        \overload
        \since 6.7

        Calls \a function with the arguments \a args in the context of \a context with
        \c Qt::AutoConnection and stores the result in the variable that \a ret refers to.
    */
    template <typename Functor, typename Ret, typename... Args,
              std::enable_if_t<QtPrivate::CanInvoke<Functor, Args...>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, QTemplatedMetaMethodReturnArgument<Ret> ret, Args &&...args) {
        return invokeFunction(context, std::forward<Functor>(function), Qt::AutoConnection,
                              static_cast<Ret *>(ret.data), std::forward<Args>(args)...);
    }

    /*!
        \overload
        \since 5.10

        Calls \a function, a pointer to a member function without parameters, on \a context
        using the connection type \a type, and stores the result in \a ret.
    */
    template <typename Functor, typename Ret,
              std::enable_if_t<
                  std::is_member_function_pointer_v<std::decay_t<Functor>> && QtPrivate::CanInvoke<Functor>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, Qt::ConnectionType type, Ret *ret) {
        return invokeFunction(context, std::forward<Functor>(function), type, ret);
    }

    /*!
        \overload
        \since 5.10

        Calls \a function, a pointer to a member function without parameters, on \a context
        with \c Qt::AutoConnection and stores the result in \a ret.
    */
    template <typename Functor, typename Ret,
              std::enable_if_t<
                  std::is_member_function_pointer_v<std::decay_t<Functor>> && QtPrivate::CanInvoke<Functor>, int> = 0>
    static bool invokeMethod(typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                             Functor &&function, Ret *ret) {
        return invokeFunction(context, std::forward<Functor>(function), Qt::AutoConnection, ret);
    }

    QObject *newInstance(QGenericArgument val0 = QGenericArgument(nullptr), QGenericArgument val1 = QGenericArgument(),
                         QGenericArgument val2 = QGenericArgument(), QGenericArgument val3 = QGenericArgument(),
                         QGenericArgument val4 = QGenericArgument(), QGenericArgument val5 = QGenericArgument(),
                         QGenericArgument val6 = QGenericArgument(), QGenericArgument val7 = QGenericArgument(),
                         QGenericArgument val8 = QGenericArgument(), QGenericArgument val9 = QGenericArgument()) const;

    /*!
        The meta-object data that \c ntmoc generates. It is public so that generated code can
        initialize it, and is not part of the API.
    */
    struct {
        const QMetaObject *superdata;
        const QtPrivate::MetaObjectData *data;
        StaticMetacallFunction static_metacall;
    } d;

private:
    static bool invokeFunctor(QObject *context, std::function<void()> function, Qt::ConnectionType type);
    // Returns Qt::DirectConnection, Qt::QueuedConnection or Qt::BlockingQueuedConnection: how a
    // call of the given type to an object in the thread of context is made from this thread.
    static Qt::ConnectionType resolveConnectionType(const QObject *context, Qt::ConnectionType type);

    // The type of the variable that can receive the result of calling Functor with Args.
    template <typename Functor, typename... Args>
    using InvokeResult =
        std::remove_cv_t<std::remove_reference_t<typename QtPrivate::InvokeResultOf<Functor, Args...>::type>>;

    // Calls function with args, on context if it is a member function.
    template <typename Context, typename Functor, typename Ret, typename... Args>
    static bool invokeFunction(Context *context, Functor &&function, Qt::ConnectionType type, Ret *ret,
                               Args &&...args) {
        if constexpr (std::is_member_function_pointer_v<std::decay_t<Functor>>)
            return invokeCallable(context, std::forward<Functor>(function), type, ret, context,
                                  std::forward<Args>(args)...);
        else
            return invokeCallable(context, std::forward<Functor>(function), type, ret, std::forward<Args>(args)...);
    }

    // Calls function with args in the context of context, and stores the result in *ret unless
    // ret is null.
    template <typename Functor, typename Ret, typename... Args>
    static bool invokeCallable(QObject *context, Functor &&function, Qt::ConnectionType type, Ret *ret,
                               Args &&...args) {
        if (!context)
            return false;
        type = resolveConnectionType(context, type);
        if (type == Qt::QueuedConnection) {
            if (ret)
                return false;
            // The call runs after this function has returned, so it owns the function and the
            // arguments.
            auto call = std::make_shared<std::pair<std::decay_t<Functor>, std::tuple<std::decay_t<Args>...>>>(
                std::piecewise_construct, std::forward_as_tuple(std::forward<Functor>(function)),
                std::forward_as_tuple(std::forward<Args>(args)...));
            return invokeFunctor(
                context,
                [call]() {
                    // Each copy is passed on as the argument was given: as an lvalue or an rvalue.
                    std::apply(
                        [&call](std::decay_t<Args> &...values) {
                            (void)std::invoke(
                                call->first,
                                static_cast<std::conditional_t<std::is_lvalue_reference_v<Args>, std::decay_t<Args> &,
                                                               std::decay_t<Args> &&>>(values)...);
                        },
                        call->second);
                },
                type);
        }
        // A direct or blocking call is over when invokeFunctor() returns, so references do.
        return invokeFunctor(
            context,
            [&]() {
                if constexpr (std::is_void_v<std::invoke_result_t<Functor, Args...>>) {
                    std::invoke(std::forward<Functor>(function), std::forward<Args>(args)...);
                } else {
                    if (ret)
                        *ret = std::invoke(std::forward<Functor>(function), std::forward<Args>(args)...);
                    else
                        (void)std::invoke(std::forward<Functor>(function), std::forward<Args>(args)...);
                }
            },
            type);
    }
};

namespace QtPrivate {

// Called from the initializer of a class's staticMetaObject: links the class to its type so
// that QObject::metaObject() can find the dynamic type's meta-object. Returns `super`.
// `isInstance` tells whether an object is of the class; it is null for a gadget.
const QMetaObject *registerMetaObject(const std::type_info &type, const QMetaObject *self, const QMetaObject *super,
                                      bool (*isInstance)(const QObject *));
// Returns nullptr if no meta-object was registered for `type`.
const QMetaObject *findMetaObject(const std::type_info &type);
// The meta-object of the dynamic type of `object`.
const QMetaObject *findMetaObjectOf(const QObject *object);

template <typename P> P metaArg(void *p) { return static_cast<P>(*static_cast<std::remove_reference_t<P> *>(p)); }

} // namespace QtPrivate
