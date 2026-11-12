// MIT License - see LICENSE
//
// The QML engine of Nt Qml. A QML document is parsed into a tree (qjsparser_p.h), and a Creator builds the
// objects it declares: C++ types registered with qmlRegisterType(), or types written in QML files. What a
// QObject lacks in C++ but has in QML (declared properties, signals and functions, bindings, handlers) is
// kept in its ObjectData, and reached from scripts through hooks of the JavaScript runtime.
//
// A binding is a function that is called again whenever a property it read changes: a C++ property with a
// NOTIFY signal, a property declared in QML, or a context property.
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qset.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlerror.h>

#include "qjsparser_p.h"
#include "qjsruntime_p.h"

#include <functional>
#include <memory>
#include <vector>

namespace QtQmlPrivate {

using QtJSPrivate::Env;
using QtJSPrivate::Function;
using QtJSPrivate::Object;
using QtJSPrivate::ProgramData;
using QtJSPrivate::QmlObjectNode;
using QtJSPrivate::Runtime;
using QtJSPrivate::Value;
using QtJSPrivate::Values;

// A type that QML documents can use.
struct TypeInfo {
    QString module; // "QtQml"
    int major = 1;
    int minor = 0;
    QString name; // "Timer"
    const QMetaObject *metaObject = nullptr;
    std::function<QObject *()> create; // null for uncreatable types
    QString noCreationReason;
    bool singleton = false;
    std::function<QObject *(QQmlEngine *, QJSEngine *)> singletonFactory;
    QPointer<QObject> singletonInstance;
    QUrl url; // a type written in QML
};

class TypeRegistry {
public:
    static TypeRegistry &instance();
    void add(const TypeInfo &type);
    // The type `name` of `module`, or nullptr.
    const TypeInfo *find(const QString &module, const QString &name) const;
    bool hasModule(const QString &module) const;
    QList<TypeInfo> types;
    QStringList modules;
};

// A property declared in QML: "property int count: 3".
struct DeclaredProperty {
    QString name;
    QString type;
    Value value;
    bool readonly = false;
    // An alias reads and writes another property, or is another object.
    bool alias = false;
    QPointer<QObject> aliasObject;
    QString aliasProperty;
};

struct Binding;

// Something a binding read, and is called again for when it changes.
struct Dependency {
    QMetaObject::Connection connection; // the NOTIFY signal of a C++ property
    QString key;                        // a declared or context property, in EngineData::dependents
};

struct Binding {
    QPointer<QObject> target;
    QString property;
    Function *function = nullptr;
    Value thisObject;
    QString location; // "file:line", for messages
    std::vector<Dependency> dependencies;
    bool evaluating = false;
};

// A handler of a signal declared in QML, or a function connected to it with connect().
struct Handler {
    Value thisObject;
    Function *function = nullptr;
    const QtJSPrivate::FunctionNode *node = nullptr; // a handler written as "onSignal: ..."
    std::shared_ptr<const ProgramData> program;
    Env *scope = nullptr;
};

struct ObjectData {
    QPointer<QQmlContext> context;
    Env *scope = nullptr; // where its bindings and functions look names up
    std::vector<DeclaredProperty> properties;
    QHash<QString, QStringList> signalParameters; // signals declared in QML, with their parameter names
    QHash<QString, Function *> methods;           // functions declared in QML, and the functions of signals
    QHash<QString, std::vector<Handler>> handlers;
    QHash<QString, Binding *> bindings;
    QHash<QString, QHash<QString, int>> enums;
    QString defaultProperty; // declared in QML with "default property"
    std::vector<Function *> onCompleted;
    std::vector<Function *> onDestruction;
    std::vector<QMetaObject::Connection> connections; // handlers of C++ signals
    ~ObjectData();
    DeclaredProperty *property(const QString &name);
};

// A QML document: its source tree and where it came from.
struct Document {
    std::shared_ptr<const ProgramData> program;
    QUrl url;
};

class EngineData {
public:
    explicit EngineData(QQmlEngine *engine);
    ~EngineData();
    EngineData(const EngineData &) = delete;
    EngineData &operator=(const EngineData &) = delete;

    static EngineData *of(QQmlEngine *engine) { return engine ? engine->m_data.get() : nullptr; }
    static EngineData *of(const QObject *object);

    QQmlEngine *engine;
    Runtime *rt;
    QQmlContext *rootContext = nullptr;
    QStringList importPaths;
    QUrl baseUrl;
    bool outputWarnings = true;
    QHash<QString, std::shared_ptr<const ProgramData>> documents; // parsed documents, by URL

    // Objects made by QML, and what they have in QML.
    QHash<QObject *, ObjectData *> objects;
    ObjectData *data(QObject *object, bool create);

    // The scope of a context in the runtime: its ids, context properties and context object.
    Object *contextScope(QQmlContext *context);
    Env *contextEnv(QQmlContext *context);
    void setContextEnv(QQmlContext *context, Env *env);
    // Set by Nt Quick: makes `parent` the visual parent of `object` when a script makes it with createObject().
    std::function<void(QObject *object, QObject *parent)> setVisualParent;
    QObject *singleton(const QString &module, const QString &name);
    // Creators at work, whose values are roots.
    QSet<Creator *> creators;
    // Singletons written in QML, one instance per engine, by URL.
    QHash<QString, QPointer<QObject>> singletons;

    // Records that the binding being evaluated read `key` (a declared or context property).
    void noteRead(const QString &key);
    // What a context has of its own, for its scope.
    static QObject *contextId(QQmlContext *context, const QString &name);
    static const QHash<QString, QVariant> &contextProperties(QQmlContext *context);
    // Mirrors declared properties into dynamic properties, and back.
    QObject *watcher = nullptr;
    bool mirroring = false;
    void mirror(QObject *object, const QString &name, const Value &value);
    bool m_tearingDown = false;

    // Properties declared in QML.
    bool getDeclared(QObject *object, const QString &name, Value *out);
    bool setDeclared(QObject *object, const QString &name, const Value &value, bool fromBinding);
    void declareProperty(QObject *object, const DeclaredProperty &property);
    void declareSignal(QObject *object, const QString &name, const QStringList &parameters);
    // Calls the handlers of the QML signal `name` of `object`.
    void emitDeclared(QObject *object, const QString &name, const Values &args);

    // Writes `value` to the property `name` of `object`, C++ or QML; `name` may be grouped ("anchors.fill").
    bool writeProperty(QObject *object, const QString &name, const Value &value, QString *error);
    Value readProperty(QObject *object, const QString &name);

    // Bindings.
    void setBinding(QObject *object, const QString &name, Binding *binding);
    void removeBinding(QObject *object, const QString &name);
    void evaluate(Binding *binding);
    void changed(const QString &key);
    static QString key(const void *owner, const QString &name);

    // Handlers of C++ signals and of QML signals.
    bool connectHandler(QObject *object, const QString &signal, const Handler &handler, QString *error);
    void callHandler(const Handler &handler, const Values &args, const QStringList &names);

    void warn(const QQmlError &error);
    void warn(const QString &location, const QString &message);
    QString errorText(const Value &error);

    // Loading documents.
    std::shared_ptr<const ProgramData> load(const QUrl &url, QList<QQmlError> *errors);
    std::shared_ptr<const ProgramData> parse(const QByteArray &data, const QUrl &url, QList<QQmlError> *errors);
    QUrl resolve(const QUrl &base, const QUrl &relative) const;

private:
    void installHooks();
    void installQtObject();
    void markRoots(std::vector<QtJSPrivate::Cell *> *roots) const;

    std::vector<Binding *> m_capture; // the bindings being evaluated, innermost last
    QSet<Binding *> m_live;
    QHash<QString, QSet<Binding *>> m_dependents;
    QHash<QQmlContext *, Object *> m_contextScopes;
    QHash<QQmlContext *, Env *> m_contextEnvs;
    std::vector<Function *> m_callLater;
    bool m_callLaterQueued = false;
};

struct Imports;

// Builds the objects of one component: creates them, then sets their bindings, and completes them.
class Creator {
public:
    Creator(QQmlEngine *engine, const Document &document);
    ~Creator();
    Creator(const Creator &) = delete;
    Creator &operator=(const Creator &) = delete;

    // Creates the object declared by `node` (the root of the document if null) in `context`.
    QObject *create(const QmlObjectNode *node, QQmlContext *context, const QVariantMap &initialProperties);
    // Sets the bindings and completes the objects created so far.
    void complete();

    QList<QQmlError> errors;
    // Values the creator holds until it completes; the engine marks them as roots.
    std::vector<Object *> keep;

private:
    struct TypeRef {
        const TypeInfo *cpp = nullptr;
        std::shared_ptr<const ProgramData> program; // a type written in QML
        QUrl url;
        const QmlObjectNode *inlineNode = nullptr; // an inline component of this document
        bool isComponent = false;                  // "Component { }"
    };
    struct Pending {
        QPointer<QObject> object;
        std::function<void()> apply;
    };

    TypeRef resolveType(const QString &name, int line);
    QObject *instantiate(const QmlObjectNode *node, QQmlContext *context, QObject *parent, QObject **root);
    Env *contextScopeEnv(QQmlContext *context, QObject *root);
    void applyMembers(QObject *object, const QmlObjectNode *node, QQmlContext *context, Env *scope, QObject *root);
    void applyBinding(QObject *object, const QtJSPrivate::QmlBinding *b, QQmlContext *context, Env *scope,
                      QObject *root);
    void resolveAlias(QObject *owner, const QString &name, QObject *target, const QString &property);
    void addToDefault(QObject *parent, QObject *child, int line);
    void completeObjects();
    void collectCreated(std::vector<QPointer<QObject>> *out) const;
    void fail(int line, const QString &message);
    QString location(int line) const;

    QPointer<QQmlEngine> m_engine;
    EngineData *m_data;
    Document m_doc;
    std::shared_ptr<Imports> m_imports;
    Object *m_typeScope = nullptr;
    int m_depth = 0;
    std::vector<Pending> m_aliases;
    std::vector<Pending> m_values;
    std::vector<Pending> m_handlers;
    std::vector<Pending> m_pending;
    std::vector<Pending> m_initial;
    std::vector<QPointer<QObject>> m_created;
    std::vector<std::unique_ptr<Creator>> m_owned;
    bool m_completed = false;
    bool m_completeObjects = true;
};

// The QML type of an object, for messages.
QString typeName(const QObject *object);

} // namespace QtQmlPrivate
