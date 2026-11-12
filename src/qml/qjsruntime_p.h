// MIT License - see LICENSE
//
// The JavaScript runtime of Nt Qml: values, objects, scopes, the garbage-collected heap and the interpreter
// state. The parser (qjsparser_p.h) turns source into a tree, and the interpreter (qjsinterp.cpp) walks it.
//
// Memory: every object and scope is a Cell owned by the Heap. Collection is mark and sweep, and runs only
// when no script is running (Runtime::depth == 0), so values held in C++ locals during evaluation are never
// collected. Roots are the global object and scope, the prototypes, values held by QJSValue (persistent
// counts), and the roots that the QML engine registers.
//
// Errors thrown by scripts travel as C++ exceptions of type Throw, and are caught where the runtime returns
// to C++ code that is not part of it.
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qset.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qvariant.h>

#include <functional>
#include <memory>
#include <vector>

class QObject;
class QJSEngine;
struct QJSValuePrivate;

namespace QtJSPrivate {

class Object;
class Function;
class Env;
class Heap;
class Runtime;
struct FunctionNode;
struct ProgramData;

class Cell {
public:
    Cell() = default;
    Cell(const Cell &) = delete;
    Cell &operator=(const Cell &) = delete;
    virtual ~Cell();
    // Adds the cells this one refers to, for marking.
    virtual void markChildren(std::vector<Cell *> *stack) const = 0;
    bool marked = false;
};

class Value {
public:
    enum Type : unsigned char { Undefined, Null, Boolean, Number, String, ObjectType };

    Value() = default;
    static Value null() {
        Value v;
        v.m_type = Null;
        return v;
    }
    static Value boolean(bool b) {
        Value v;
        v.m_type = Boolean;
        v.m_bool = b;
        return v;
    }
    static Value number(double n) {
        Value v;
        v.m_type = Number;
        v.m_number = n;
        return v;
    }
    static Value string(const QString &s) {
        Value v;
        v.m_type = String;
        v.m_string = s;
        return v;
    }
    static Value object(Object *o);

    Type type() const { return m_type; }
    bool isUndefined() const { return m_type == Undefined; }
    bool isNull() const { return m_type == Null; }
    bool isNullish() const { return m_type == Undefined || m_type == Null; }
    bool isBoolean() const { return m_type == Boolean; }
    bool isNumber() const { return m_type == Number; }
    bool isString() const { return m_type == String; }
    bool isObject() const { return m_type == ObjectType; }

    bool asBoolean() const { return m_bool; }
    double asNumber() const { return m_number; }
    const QString &asString() const { return m_string; }
    Object *asObject() const { return m_object; }
    Function *asFunction() const; // nullptr unless the value is a function

    void mark(std::vector<Cell *> *stack) const;

private:
    Type m_type = Undefined;
    bool m_bool = false;
    double m_number = 0.0;
    Object *m_object = nullptr;
    QString m_string;
};

using Values = std::vector<Value>;

// What a script throws.
struct Throw {
    Value value;
};

struct Property {
    Value value;
    Function *getter = nullptr;
    Function *setter = nullptr;
    bool accessor = false;
    bool enumerable = true;
    bool writable = true;
    bool configurable = true;
};

class Object : public Cell {
public:
    enum class Class : unsigned char {
        Object,
        Array,
        Function,
        Error,
        Boolean,
        Number,
        String,
        Date,
        RegExp,
        Arguments,
        Host
    };

    explicit Object(Object *prototype, Class c = Class::Object) : proto(prototype), cls(c) {}

    Object *proto = nullptr;
    Class cls = Class::Object;
    bool extensible = true;
    bool frozen = false;
    // Wrapped value of Boolean, Number, String and Date objects; for errors, nothing.
    Value primitive;
    // Elements of arrays, by index; arrays have no holes.
    Values elements;
    // A value from C++ that has no JavaScript form, carried as it is (Class::Host with no other meaning).
    QVariant variant;

    // Own properties in the order they were added.
    Property *ownProperty(const QString &key);
    const Property *ownProperty(const QString &key) const;
    Property &defineOwn(const QString &key);
    void setOwn(const QString &key, const Value &value, bool enumerable = true);
    bool removeOwn(const QString &key);
    QStringList ownPropertyKeys() const;

    // The operations scripts use. Host objects (QObject wrappers, QML scopes) override them.
    virtual bool getOwn(Runtime &rt, const QString &key, const Value &receiver, Value *out);
    virtual bool setOwn(Runtime &rt, const QString &key, const Value &value, const Value &receiver);
    virtual bool hasOwn(Runtime &rt, const QString &key);
    virtual bool deleteOwn(Runtime &rt, const QString &key);
    // Enumerable own keys, array indices first.
    virtual QStringList enumerableKeys(Runtime &rt);

    void markChildren(std::vector<Cell *> *stack) const override;

private:
    std::vector<std::pair<QString, Property>> m_props;
    QHash<QString, std::size_t> m_index;
};

class Function : public Object {
public:
    // A function written in C++. `newTarget` is set when it is called with `new`.
    using Native = std::function<Value(Runtime &rt, const Value &thisValue, const Values &args, Object *newTarget)>;

    explicit Function(Object *prototype) : Object(prototype, Class::Function) {}

    Native native;
    // A function written in JavaScript.
    const FunctionNode *node = nullptr;
    std::shared_ptr<const ProgramData> program;
    Env *scope = nullptr;
    // A function made by bind().
    Function *target = nullptr;
    Value boundThis;
    Values boundArgs;
    bool constructor = true;
    // Called on the home object for `super` lookups; unused for now.
    QString name;

    void markChildren(std::vector<Cell *> *stack) const override;
};

// A QObject seen from scripts: its properties, methods and signals by name. Properties that scripts add
// are kept on the wrapper.
class QObjectWrapper : public Object {
public:
    QObjectWrapper(Object *prototype, QObject *o);

    QPointer<QObject> object;
    QObject *key; // the address it was made for, valid as a key after the object is gone
    // Functions made for methods and signals, by name, so that obj.f === obj.f.
    QHash<QString, Function *> methods;

    bool getOwn(Runtime &rt, const QString &key, const Value &receiver, Value *out) override;
    bool setOwn(Runtime &rt, const QString &key, const Value &value, const Value &receiver) override;
    bool hasOwn(Runtime &rt, const QString &key) override;
    bool deleteOwn(Runtime &rt, const QString &key) override;
    QStringList enumerableKeys(Runtime &rt) override;
    void markChildren(std::vector<Cell *> *stack) const override;
};

// A scope: declared names, or the properties of an object (the global object, a QML scope).
class Env : public Cell {
public:
    struct Binding {
        Value value;
        bool mutableBinding = true;
        bool initialized = true;
    };

    explicit Env(Env *outer) : parent(outer) {}

    Env *parent = nullptr;
    Object *object = nullptr;
    QHash<QString, Binding> vars;
    // Function scopes set the value of `this`; arrow functions and blocks do not.
    bool hasThis = false;
    Value thisValue;
    Function *callee = nullptr;
    Values arguments;

    void markChildren(std::vector<Cell *> *stack) const override;
};

class Heap {
public:
    Heap() = default;
    Heap(const Heap &) = delete;
    Heap &operator=(const Heap &) = delete;
    ~Heap();

    template <typename T, typename... Args> T *make(Args &&...args) {
        T *cell = new T(std::forward<Args>(args)...);
        m_cells.push_back(cell);
        ++m_sinceCollect;
        return cell;
    }
    std::size_t sinceCollect() const { return m_sinceCollect; }
    std::size_t size() const { return m_cells.size(); }
    // Frees what cannot be reached from `roots`. `beforeFree` sees each cell about to be freed.
    void collect(const std::vector<Cell *> &roots, const std::function<void(Cell *)> &beforeFree);

private:
    std::vector<Cell *> m_cells;
    std::size_t m_sinceCollect = 0;
};

enum class ErrorType { Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError };

class Runtime {
public:
    explicit Runtime(QJSEngine *engine);
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    ~Runtime();

    QJSEngine *engine;
    Heap heap;
    Object *global = nullptr;
    Env *globalEnv = nullptr;
    Object *objectProto = nullptr;
    Function *functionProto = nullptr;
    Object *arrayProto = nullptr;
    Object *stringProto = nullptr;
    Object *numberProto = nullptr;
    Object *booleanProto = nullptr;
    Object *dateProto = nullptr;
    Object *regexpProto = nullptr;
    Object *errorProto = nullptr;
    Object *errorProtos[7] = {};
    Object *qobjectProto = nullptr;
    // How deep the interpreter is: in calls, and in nested evaluations of the tree.
    int depth = 0;
    int callDepth = 0;
    // The QJSValues of this runtime, which are reset when the runtime goes.
    QSet<QJSValuePrivate *> handles;
    // A script stops at its next statement when this is set (QJSEngine::setInterrupted()).
    bool interrupted = false;
    // An error that C++ code raised with QJSEngine::throwError(), thrown when control returns to the script.
    bool hasPendingError = false;
    Value pendingError;
    // Values that C++ holds on to (QJSValue), counted.
    QHash<Object *, int> persistent;
    // Further roots, such as the scopes and objects of QML contexts.
    std::vector<std::function<void(std::vector<Cell *> *)>> rootProviders;
    // Wrappers of QObjects, one per object.
    QHash<QObject *, Object *> wrappers;
    // QObjects that scripts own: deleted when their wrapper is collected without a parent.
    QHash<QObject *, bool> scriptOwned;
    // Connections from signals to script functions; the functions stay alive while connected.
    struct ScriptConnection {
        QPointer<QObject> sender;
        int signalIndex = -1;
        Value thisObject;
        Function *function = nullptr;
        QMetaObject::Connection connection;
    };
    std::vector<ScriptConnection> connections;
    // The receiver of those connections: they end when it is destroyed.
    QObject *connectionContext = nullptr;
    // Called with errors that scripts throw where no script can catch them, such as in a signal handler.
    std::function<void(const Value &error)> reportError;
    // The QML engine: properties, signals and functions declared in QML, which QObjects do not have in C++
    // (qmlGet, qmlSet, qmlHas); reads of properties, which bindings depend on (qmlRead); and writes of
    // C++ properties from scripts, which replace a binding (qmlWrite).
    std::function<bool(QObject *object, const QString &name, Value *out)> qmlGet;
    std::function<bool(QObject *object, const QString &name, const Value &value)> qmlSet;
    std::function<bool(QObject *object, const QString &name)> qmlHas;
    std::function<void(QObject *object, const QString &name, int propertyIndex)> qmlRead;
    std::function<void(QObject *object, const QString &name)> qmlWrite;
    // Conversions of C++ types that the runtime does not know, added by the QML engine.
    std::function<bool(const QVariant &variant, Value *out)> fromVariantHook;
    std::function<bool(const Value &value, QMetaType target, QVariant *out)> toVariantHook;
    // Source of the error being thrown, for stack traces.
    QString currentFile;
    int currentLine = 0;
    QStringList stack;

    // Making values.
    Object *newObject() { return heap.make<Object>(objectProto); }
    Object *newArray(const Values &elements = {});
    Function *newNative(const QString &name, int length, Function::Native fn, bool constructor = false);
    Object *newError(ErrorType type, const QString &message);
    [[noreturn]] void throwError(ErrorType type, const QString &message);
    [[noreturn]] void throwTypeError(const QString &message) { throwError(ErrorType::TypeError, message); }

    // Conversions, as the language defines them.
    bool toBoolean(const Value &v) const;
    double toNumber(const Value &v);
    QString toString(const Value &v);
    Object *toObject(const Value &v);
    Value toPrimitive(const Value &v, bool preferString);
    QString typeOf(const Value &v) const;
    static double stringToNumber(const QString &s);
    static QString numberToString(double d);

    // Property access on any value.
    Value get(const Value &base, const QString &key);
    void put(const Value &base, const QString &key, const Value &value);
    Value getFromObject(Object *object, const QString &key, const Value &receiver);
    bool putToObject(Object *object, const QString &key, const Value &value, const Value &receiver);
    bool has(Object *object, const QString &key);

    // Calls.
    Value call(const Value &function, const Value &thisValue, const Values &args);
    Value construct(const Value &function, const Values &args, Object *newTarget = nullptr);
    bool isCallable(const Value &v) const { return v.asFunction() != nullptr; }

    // Comparisons.
    bool strictEquals(const Value &a, const Value &b) const;
    bool looseEquals(const Value &a, const Value &b);
    bool sameValueZero(const Value &a, const Value &b) const;

    // Running source code. `scope` defaults to the global scope; `thisValue` to undefined.
    Value evaluate(const QString &source, const QString &fileName, int line, Env *scope = nullptr,
                   const Value &thisValue = Value());
    // Collects garbage if enough was allocated, when nothing runs.
    void maybeCollect();
    void collect();

    // C++ values.
    Value fromVariant(const QVariant &variant);
    QVariant toVariant(const Value &value, QMetaType target = QMetaType());
    Value wrapQObject(QObject *object);
    QObject *unwrapQObject(const Value &value) const;

    // Raised by an object that knows no QObject wrapper prototype yet; set up in qjsqobject.cpp.
    void installQObjectSupport();
    void installBuiltins();
};

// Array index parsing: "0", "17", but not "01" or "-1".
bool isArrayIndex(const QString &key, quint32 *index);

// RAII depth counter for nested evaluation.
struct DepthGuard {
    explicit DepthGuard(Runtime &r);
    ~DepthGuard() { --rt.depth; }
    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;
    Runtime &rt;
};

} // namespace QtJSPrivate
