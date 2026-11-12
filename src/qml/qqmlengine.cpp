// MIT License - see LICENSE
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlerror.h>
#include <QtQml/qqmlparserstatus.h>

#include "qjsbuiltins_p.h"
#include "qjsengine_p.h"
#include "qjsinterp_p.h"
#include "qqmlengine_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qtimer.h>

#include <cmath>

using QtJSPrivate::EngineAccess;
using QtJSPrivate::Throw;

namespace QtQmlPrivate {

namespace {

// Which context each object made by QML belongs to; read by QQmlEngine::contextForObject() from any engine.
QMutex g_contextsMutex;
QHash<const QObject *, QPointer<QQmlContext>> &contexts() {
    static QHash<const QObject *, QPointer<QQmlContext>> map;
    return map;
}

// The scope of a context: its ids, its context properties and the properties of its context object.
class ContextScope : public Object {
public:
    ContextScope(Object *prototype, QQmlContext *c, EngineData *d)
        : Object(prototype, Class::Host), context(c), data(d) {}

    bool getOwn(Runtime &rt, const QString &key, const Value &receiver, Value *out) override {
        QQmlContext *c = context.data();
        if (!c)
            return false;
        if (QObject *o = EngineData::contextId(c, key)) {
            *out = rt.wrapQObject(o);
            return true;
        }
        const QHash<QString, QVariant> &properties = EngineData::contextProperties(c);
        if (properties.contains(key)) {
            data->noteRead(EngineData::key(c, key));
            *out = rt.fromVariant(properties.value(key));
            return true;
        }
        if (QObject *o = c->contextObject()) {
            const Value w = rt.wrapQObject(o);
            if (w.asObject()->hasOwn(rt, key)) {
                *out = rt.getFromObject(w.asObject(), key, w);
                return true;
            }
        }
        return Object::getOwn(rt, key, receiver, out);
    }
    bool hasOwn(Runtime &rt, const QString &key) override {
        QQmlContext *c = context.data();
        if (!c)
            return false;
        if (EngineData::contextId(c, key) || EngineData::contextProperties(c).contains(key))
            return true;
        if (QObject *o = c->contextObject())
            if (rt.wrapQObject(o).asObject()->hasOwn(rt, key))
                return true;
        return Object::hasOwn(rt, key);
    }
    bool setOwn(Runtime &rt, const QString &key, const Value &value, const Value &receiver) override {
        QQmlContext *c = context.data();
        if (c && (EngineData::contextId(c, key) || EngineData::contextProperties(c).contains(key)))
            rt.throwTypeError(QStringLiteral("cannot assign to '%1': it is an id or a context property").arg(key));
        if (c && c->contextObject()) {
            const Value w = rt.wrapQObject(c->contextObject());
            if (w.asObject()->hasOwn(rt, key))
                return w.asObject()->setOwn(rt, key, value, w);
        }
        return Object::setOwn(rt, key, value, receiver);
    }

    QPointer<QQmlContext> context;
    EngineData *data;
};

// Mirrors properties that C++ sets with QObject::setProperty() into the QML properties of the same name.
class DynamicPropertyWatcher : public QObject {
public:
    explicit DynamicPropertyWatcher(EngineData *d) : data(d) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::DynamicPropertyChange && !data->mirroring) {
            const auto *e = static_cast<QDynamicPropertyChangeEvent *>(event);
            const QString name = QString::fromUtf8(e->propertyName());
            ObjectData *d = data->objects.value(watched);
            if (d && d->property(name)) {
                data->mirroring = true;
                data->setDeclared(watched, name,
                                  data->rt->fromVariant(watched->property(e->propertyName().constData())), false);
                data->mirroring = false;
            }
        }
        return false;
    }
    EngineData *data;
};

QString capitalized(const QString &s) { return s.isEmpty() ? s : s.at(0).toUpper() + s.mid(1); }

bool isListProperty(const QMetaProperty &p) { return QByteArray(p.typeName()).startsWith("QQmlListProperty<"); }

} // namespace

QString typeName(const QObject *object) {
    if (!object)
        return QStringLiteral("null");
    for (const TypeInfo &t : TypeRegistry::instance().types)
        if (t.metaObject == object->metaObject() && !t.name.isEmpty())
            return t.name;
    return QString::fromLatin1(object->metaObject()->className());
}

ObjectData::~ObjectData() {
    for (const QMetaObject::Connection &c : connections)
        QObject::disconnect(c);
}

DeclaredProperty *ObjectData::property(const QString &name) {
    for (DeclaredProperty &p : properties)
        if (p.name == name)
            return &p;
    return nullptr;
}

// ----------------------------------------------------------------------------------------------------------------

EngineData::EngineData(QQmlEngine *e) : engine(e), rt(EngineAccess::runtime(e)) {
    watcher = new DynamicPropertyWatcher(this);
    watcher->setParent(e);
    installHooks();
    installQtObject();
    rt->rootProviders.emplace_back([this](std::vector<QtJSPrivate::Cell *> *roots) { markRoots(roots); });
}

EngineData::~EngineData() {
    m_tearingDown = true;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        ObjectData *d = it.value();
        for (Binding *b : std::as_const(d->bindings)) {
            for (const Dependency &dep : b->dependencies)
                QObject::disconnect(dep.connection);
            delete b;
        }
        delete d;
    }
    objects.clear();
    rt->qmlGet = nullptr;
    rt->qmlSet = nullptr;
    rt->qmlHas = nullptr;
    rt->qmlRead = nullptr;
    rt->qmlWrite = nullptr;
}

EngineData *EngineData::of(const QObject *object) {
    QQmlContext *c = QQmlEngine::contextForObject(object);
    return c ? of(c->engine()) : nullptr;
}

QString EngineData::key(const void *owner, const QString &name) {
    return QString::number(reinterpret_cast<quintptr>(owner), 16) + u':' + name;
}

ObjectData *EngineData::data(QObject *object, bool create) {
    if (ObjectData *d = objects.value(object))
        return d;
    if (!create)
        return nullptr;
    auto *d = new ObjectData;
    objects.insert(object, d);
    QObject::connect(object, &QObject::destroyed, engine, [this, object] {
        if (m_tearingDown)
            return;
        ObjectData *gone = objects.take(object);
        if (!gone)
            return;
        for (Function *f : gone->onDestruction) {
            try {
                rt->call(Value::object(f), Value(), {});
            } catch (const Throw &t) {
                warn(QString(), errorText(t.value));
            }
        }
        for (auto it = gone->bindings.begin(); it != gone->bindings.end(); ++it) {
            Binding *b = it.value();
            m_live.remove(b);
            for (const Dependency &dep : b->dependencies) {
                QObject::disconnect(dep.connection);
                if (!dep.key.isEmpty())
                    m_dependents[dep.key].remove(b);
            }
            delete b;
        }
        delete gone;
        QMutexLocker lock(&g_contextsMutex);
        contexts().remove(object);
    });
    return d;
}

void EngineData::markRoots(std::vector<QtJSPrivate::Cell *> *roots) const {
    const auto mark = [roots](const Value &v) {
        if (v.isObject())
            roots->push_back(v.asObject());
    };
    for (const ObjectData *d : objects) {
        if (d->scope)
            roots->push_back(d->scope);
        for (const DeclaredProperty &p : d->properties)
            mark(p.value);
        for (Function *f : d->methods)
            roots->push_back(f);
        for (const auto &list : d->handlers)
            for (const Handler &h : list) {
                mark(h.thisObject);
                if (h.function)
                    roots->push_back(h.function);
                if (h.scope)
                    roots->push_back(h.scope);
            }
        for (const Binding *b : d->bindings) {
            if (b->function)
                roots->push_back(b->function);
            mark(b->thisObject);
        }
        for (Function *f : d->onCompleted)
            roots->push_back(f);
        for (Function *f : d->onDestruction)
            roots->push_back(f);
    }
    for (Object *o : m_contextScopes)
        roots->push_back(o);
    for (Env *e : m_contextEnvs)
        roots->push_back(e);
    for (Function *f : m_callLater)
        roots->push_back(f);
    for (const Creator *c : creators)
        for (Object *o : c->keep)
            roots->push_back(o);
    for (const Binding *b : m_capture)
        if (b->function)
            roots->push_back(b->function);
}

// ---- Contexts ----

Object *EngineData::contextScope(QQmlContext *context) {
    if (Object *o = m_contextScopes.value(context))
        return o;
    auto *scope = rt->heap.make<ContextScope>(nullptr, context, this);
    m_contextScopes.insert(context, scope);
    QObject::connect(context, &QObject::destroyed, engine, [this, context] {
        if (m_tearingDown)
            return;
        m_contextScopes.remove(context);
        m_contextEnvs.remove(context);
    });
    return scope;
}

Env *EngineData::contextEnv(QQmlContext *context) {
    if (!context)
        return rt->globalEnv;
    if (Env *e = m_contextEnvs.value(context))
        return e;
    Env *e = rt->heap.make<Env>(contextEnv(context->parentContext()));
    e->object = contextScope(context);
    m_contextEnvs.insert(context, e);
    return e;
}

void EngineData::setContextEnv(QQmlContext *context, Env *env) {
    contextScope(context); // removes the entries when the context goes
    m_contextEnvs.insert(context, env);
}

void EngineData::mirror(QObject *object, const QString &name, const Value &value) {
    mirroring = true;
    object->setProperty(name.toUtf8().constData(), rt->toVariant(value));
    mirroring = false;
}

// ---- Hooks ----

void EngineData::installHooks() {
    rt->qmlGet = [this](QObject *o, const QString &name, Value *out) {
        if (name == QLatin1String("destroy")) {
            QPointer<QObject> guard = o;
            *out =
                Value::object(rt->newNative(name, 1, [guard](Runtime &r, const Value &, const Values &args, Object *) {
                    QObject *object = guard.data();
                    if (!object)
                        return Value();
                    const double delay = args.empty() ? 0.0 : r.toNumber(args[0]);
                    if (delay > 0 && std::isfinite(delay))
                        QTimer::singleShot(static_cast<int>(std::min(delay, 2e9)), object, [guard] {
                            if (guard)
                                guard->deleteLater();
                        });
                    else
                        object->deleteLater();
                    return Value();
                }));
            return true;
        }
        return getDeclared(o, name, out);
    };
    rt->qmlSet = [this](QObject *o, const QString &name, const Value &value) {
        return setDeclared(o, name, value, false);
    };
    rt->qmlHas = [this](QObject *o, const QString &name) {
        ObjectData *d = objects.value(o);
        return d && (d->property(name) || d->methods.contains(name) || d->signalParameters.contains(name) ||
                     d->enums.contains(name));
    };
    rt->qmlRead = [this](QObject *o, const QString &name, int index) {
        if (m_capture.empty())
            return;
        Binding *b = m_capture.back();
        const QString k = key(o, name);
        for (const Dependency &dep : b->dependencies)
            if (dep.key == k)
                return;
        const QMetaProperty prop = o->metaObject()->property(index);
        Dependency dep;
        dep.key = k;
        if (prop.hasNotifySignal())
            dep.connection =
                QtPrivate::Access::connectFunction(o, prop.notifySignal(), engine, [this, b](void *const *) {
                    if (m_live.contains(b))
                        evaluate(b);
                });
        // The key marks the dependency as recorded; C++ properties are not in m_dependents.
        dep.key = QStringLiteral("cpp:") + k;
        b->dependencies.push_back(dep);
    };
    rt->qmlWrite = [this](QObject *o, const QString &name) {
        if (!m_capture.empty() && m_capture.back()->target == o && m_capture.back()->property == name)
            return;
        removeBinding(o, name);
    };
}

QObject *EngineData::contextId(QQmlContext *context, const QString &name) { return context->m_ids.value(name).data(); }

const QHash<QString, QVariant> &EngineData::contextProperties(QQmlContext *context) { return context->m_properties; }

void EngineData::noteRead(const QString &k) {
    if (m_capture.empty())
        return;
    Binding *b = m_capture.back();
    for (const Dependency &dep : b->dependencies)
        if (dep.key == k)
            return;
    Dependency dep;
    dep.key = k;
    b->dependencies.push_back(dep);
    m_dependents[k].insert(b);
}

// ---- Declared properties and signals ----

void EngineData::declareProperty(QObject *object, const DeclaredProperty &property) {
    ObjectData *d = data(object, true);
    if (DeclaredProperty *existing = d->property(property.name)) {
        *existing = property;
    } else {
        d->properties.push_back(property);
        if (d->properties.size() == 1)
            object->installEventFilter(watcher);
    }
    d->signalParameters.insert(property.name + QStringLiteral("Changed"), {});
}

void EngineData::declareSignal(QObject *object, const QString &name, const QStringList &parameters) {
    data(object, true)->signalParameters.insert(name, parameters);
}

bool EngineData::getDeclared(QObject *object, const QString &name, Value *out) {
    ObjectData *d = objects.value(object);
    if (!d)
        return false;
    if (DeclaredProperty *p = d->property(name)) {
        noteRead(key(object, name));
        if (p->alias) {
            if (!p->aliasObject) {
                *out = Value();
                return true;
            }
            *out = p->aliasProperty.isEmpty() ? rt->wrapQObject(p->aliasObject.data())
                                              : readProperty(p->aliasObject.data(), p->aliasProperty);
            return true;
        }
        *out = p->value;
        return true;
    }
    if (Function *f = d->methods.value(name)) {
        *out = Value::object(f);
        return true;
    }
    if (d->signalParameters.contains(name)) {
        // The function of a signal declared in QML: calling it emits the signal.
        QPointer<QObject> guard = object;
        Function *f = rt->newNative(name, static_cast<int>(d->signalParameters.value(name).size()),
                                    [this, guard, name](Runtime &, const Value &, const Values &args, Object *) {
                                        if (guard)
                                            emitDeclared(guard.data(), name, args);
                                        return Value();
                                    });
        QtJSPrivate::addMethod(*rt, f, QStringLiteral("connect"), 1,
                               [this, guard, name](Runtime &r, const Value &, const Values &args, Object *) {
                                   ObjectData *od = guard ? objects.value(guard.data()) : nullptr;
                                   if (!od)
                                       return Value();
                                   Handler h;
                                   h.thisObject = args.size() >= 2 ? args[0] : Value();
                                   h.function =
                                       (args.size() >= 2 ? args[1] : QtJSPrivate::argument(args, 0)).asFunction();
                                   if (!h.function)
                                       r.throwTypeError(QStringLiteral("connect() needs a function"));
                                   od->handlers[name].push_back(h);
                                   return Value();
                               });
        QtJSPrivate::addMethod(*rt, f, QStringLiteral("disconnect"), 1,
                               [this, guard, name](Runtime &, const Value &, const Values &args, Object *) {
                                   ObjectData *od = guard ? objects.value(guard.data()) : nullptr;
                                   if (!od)
                                       return Value();
                                   Function *fn =
                                       (args.size() >= 2 ? args[1] : QtJSPrivate::argument(args, 0)).asFunction();
                                   auto &list = od->handlers[name];
                                   for (auto it = list.begin(); it != list.end(); ++it)
                                       if (it->function == fn) {
                                           list.erase(it);
                                           break;
                                       }
                                   return Value();
                               });
        d->methods.insert(name, f);
        *out = Value::object(f);
        return true;
    }
    if (d->enums.contains(name)) {
        Object *e = rt->newObject();
        const QHash<QString, int> values = d->enums.value(name);
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            e->setOwn(it.key(), Value::number(it.value()));
        *out = Value::object(e);
        return true;
    }
    for (auto it = d->enums.cbegin(); it != d->enums.cend(); ++it)
        if (it.value().contains(name)) {
            *out = Value::number(it.value().value(name));
            return true;
        }
    return false;
}

bool EngineData::setDeclared(QObject *object, const QString &name, const Value &value, bool fromBinding) {
    ObjectData *d = objects.value(object);
    DeclaredProperty *p = d ? d->property(name) : nullptr;
    if (!p)
        return false;
    // Qt.binding(f) makes a binding instead of a value.
    if (value.isObject()) {
        Object *o = value.asObject();
        if (const QtJSPrivate::Property *marker = o->ownProperty(QStringLiteral("__ntQmlBinding"))) {
            auto *b = new Binding;
            b->target = object;
            b->property = name;
            b->function = marker->value.asFunction();
            b->thisObject = rt->wrapQObject(object);
            setBinding(object, name, b);
            return true;
        }
    }
    if (!fromBinding)
        removeBinding(object, name);
    if (p->alias) {
        if (!p->aliasObject)
            return true;
        if (p->aliasProperty.isEmpty())
            rt->throwTypeError(QStringLiteral("cannot assign to '%1': it is an alias of an object").arg(name));
        QString error;
        if (!writeProperty(p->aliasObject.data(), p->aliasProperty, value, &error))
            rt->throwTypeError(error);
        return true;
    }
    if (p->readonly && !fromBinding)
        rt->throwTypeError(QStringLiteral("cannot assign to the read-only property '%1'").arg(name));
    Value v = value;
    const QString &t = p->type;
    if (t == QLatin1String("int")) {
        const double n = rt->toNumber(value);
        v = Value::number(std::isfinite(n) ? static_cast<double>(static_cast<qint32>(
                                                 std::clamp(std::trunc(n), -2147483648.0, 2147483647.0)))
                                           : 0.0);
    } else if (t == QLatin1String("real") || t == QLatin1String("double") || t == QLatin1String("number")) {
        v = Value::number(rt->toNumber(value));
    } else if (t == QLatin1String("bool")) {
        v = Value::boolean(rt->toBoolean(value));
    } else if (t == QLatin1String("string") || t == QLatin1String("url")) {
        v = value.isNullish() ? Value::string(QString()) : Value::string(rt->toString(value));
    } else if (t != QLatin1String("var") && t != QLatin1String("variant") && !t.startsWith(QLatin1String("list<")) &&
               !t.isEmpty() && t.at(0).isUpper() && !value.isNullish() && !rt->unwrapQObject(value)) {
        rt->throwTypeError(
            QStringLiteral("cannot assign %1 to the property '%2' of type %3").arg(rt->toString(value), name, t));
    }
    if (rt->strictEquals(p->value, v) && !(v.isNumber() && std::isnan(v.asNumber())))
        return true;
    p->value = v;
    // C++ code reads the property with QObject::property().
    mirror(object, name, v);
    changed(key(object, name));
    emitDeclared(object, name + QStringLiteral("Changed"), {});
    return true;
}

void EngineData::emitDeclared(QObject *object, const QString &name, const Values &args) {
    ObjectData *d = objects.value(object);
    if (!d)
        return;
    const std::vector<Handler> handlers = d->handlers.value(name);
    const QStringList names = d->signalParameters.value(name);
    QPointer<QObject> guard = object;
    for (const Handler &h : handlers) {
        if (!guard)
            break;
        callHandler(h, args, names);
    }
}

// ---- Properties ----

Value EngineData::readProperty(QObject *object, const QString &name) {
    if (!object)
        return Value();
    const qsizetype dot = name.indexOf(u'.');
    if (dot > 0)
        return readProperty(rt->unwrapQObject(readProperty(object, name.left(dot))), name.mid(dot + 1));
    try {
        return rt->get(rt->wrapQObject(object), name);
    } catch (const Throw &) {
        return Value();
    }
}

bool EngineData::writeProperty(QObject *object, const QString &name, const Value &value, QString *error) {
    if (!object) {
        *error = QStringLiteral("cannot assign to '%1' of null").arg(name);
        return false;
    }
    const qsizetype dot = name.indexOf(u'.');
    if (dot > 0) {
        const QString head = name.left(dot);
        if (head.at(0).isUpper()) {
            *error = QStringLiteral("attached property '%1' is not supported").arg(name);
            return false;
        }
        QObject *group = rt->unwrapQObject(readProperty(object, head));
        if (!group) {
            *error = QStringLiteral("cannot assign to '%1': '%2' is not an object").arg(name, head);
            return false;
        }
        return writeProperty(group, name.mid(dot + 1), value, error);
    }
    if (ObjectData *d = objects.value(object); d && d->property(name)) {
        try {
            return setDeclared(object, name, value, true);
        } catch (const Throw &t) {
            *error = errorText(t.value);
            return false;
        }
    }
    const QMetaObject *mo = object->metaObject();
    const int index = mo->indexOfProperty(name.toUtf8().constData());
    if (index < 0) {
        *error = QStringLiteral("cannot assign to non-existent property \"%1\"").arg(name);
        return false;
    }
    const QMetaProperty prop = mo->property(index);
    if (!prop.isWritable() && !isListProperty(prop)) {
        *error = QStringLiteral("cannot assign to the read-only property \"%1\"").arg(name);
        return false;
    }
    if (isListProperty(prop)) {
        QVariant v = prop.read(object);
        auto *list = static_cast<QQmlListProperty<QObject> *>(v.data());
        if (!list || !list->append) {
            *error = QStringLiteral("cannot assign to the list property \"%1\"").arg(name);
            return false;
        }
        if (list->clear)
            list->clear(list);
        const auto add = [&](const Value &item) {
            if (QObject *o = rt->unwrapQObject(item))
                list->append(list, o);
        };
        if (value.isObject() && value.asObject()->cls == Object::Class::Array)
            for (const Value &item : value.asObject()->elements)
                add(item);
        else
            add(value);
        return true;
    }
    QVariant v;
    try {
        if ((prop.isEnumType() || prop.isFlagType()) && value.isString()) {
            bool ok = false;
            const QByteArray keys = value.asString().toUtf8();
            const int e = prop.isFlagType() ? prop.enumerator().keysToValue(keys.constData(), &ok)
                                            : prop.enumerator().keyToValue(keys.constData(), &ok);
            if (!ok) {
                *error = QStringLiteral("'%1' is not a value of \"%2\"").arg(value.asString(), name);
                return false;
            }
            v = QVariant(e);
        } else if (prop.metaType().id() == QMetaType::QUrl && value.isString()) {
            QQmlContext *c = QQmlEngine::contextForObject(object);
            const QUrl url(value.asString());
            v = c ? c->resolvedUrl(url) : url;
        } else {
            v = rt->toVariant(value, prop.metaType());
        }
    } catch (const Throw &t) {
        *error = errorText(t.value);
        return false;
    }
    if (!prop.write(object, v)) {
        *error = QStringLiteral("cannot assign %1 to \"%2\" of type %3")
                     .arg(value.isObject() ? QStringLiteral("an object") : rt->toString(value), name,
                          QString::fromLatin1(prop.typeName()));
        return false;
    }
    return true;
}

// ---- Bindings ----

void EngineData::setBinding(QObject *object, const QString &name, Binding *binding) {
    removeBinding(object, name);
    data(object, true)->bindings.insert(name, binding);
    m_live.insert(binding);
    evaluate(binding);
}

void EngineData::removeBinding(QObject *object, const QString &name) {
    ObjectData *d = objects.value(object);
    if (!d)
        return;
    Binding *b = d->bindings.take(name);
    if (!b)
        return;
    m_live.remove(b);
    for (const Dependency &dep : b->dependencies) {
        QObject::disconnect(dep.connection);
        if (!dep.key.isEmpty())
            m_dependents[dep.key].remove(b);
    }
    // A binding that is running finishes before it goes.
    for (Binding *&running : m_capture)
        if (running == b)
            running->target = nullptr;
    if (b->evaluating)
        b->function = nullptr;
    else
        delete b;
}

void EngineData::evaluate(Binding *b) {
    if (!b->target || !b->function)
        return;
    if (b->evaluating) {
        warn(b->location, QStringLiteral("binding loop detected for property \"%1\"").arg(b->property));
        return;
    }
    for (const Dependency &dep : b->dependencies) {
        QObject::disconnect(dep.connection);
        if (!dep.key.isEmpty())
            m_dependents[dep.key].remove(b);
    }
    b->dependencies.clear();
    b->evaluating = true;
    m_capture.push_back(b);
    Value v;
    bool ok = true;
    try {
        v = rt->call(Value::object(b->function), b->thisObject, {});
    } catch (const Throw &t) {
        ok = false;
        warn(b->location, errorText(t.value));
    }
    m_capture.pop_back();
    // The write is part of the evaluation: a binding that its own write calls again is a loop.
    if (ok && b->target && b->function) {
        QString error;
        if (!writeProperty(b->target.data(), b->property, v, &error))
            warn(b->location, error);
    }
    b->evaluating = false;
    if (!b->function)
        delete b; // removed while it ran
}

void EngineData::changed(const QString &k) {
    const QSet<Binding *> list = m_dependents.value(k);
    for (Binding *b : list)
        if (m_dependents.value(k).contains(b))
            evaluate(b);
}

// ---- Handlers ----

bool EngineData::connectHandler(QObject *object, const QString &signal, const Handler &handler, QString *error) {
    ObjectData *d = data(object, true);
    if (d->signalParameters.contains(signal)) {
        d->handlers[signal].push_back(handler);
        return true;
    }
    const QMetaObject *mo = object->metaObject();
    const QByteArray name = signal.toUtf8();
    QMetaMethod method;
    for (int i = mo->methodCount() - 1; i >= 0; --i) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() == QMetaMethod::Signal && m.name() == name &&
            (!method.isValid() || m.parameterCount() > method.parameterCount()))
            method = m;
    }
    if (!method.isValid()) {
        *error = QStringLiteral("cannot assign to non-existent property \"on%1\"").arg(capitalized(signal));
        return false;
    }
    QStringList names;
    for (const QByteArray &n : method.parameterNames())
        names.append(QString::fromUtf8(n));
    // The handler is kept with the object, so that its scope stays alive.
    d->handlers[QStringLiteral("<cpp>") + signal].push_back(handler);
    d->connections.push_back(
        QtPrivate::Access::connectFunction(object, method, engine, [this, handler, method, names](void *const *argv) {
            Values args;
            for (int i = 0; i < method.parameterCount(); ++i)
                args.push_back(rt->fromVariant(QVariant(method.parameterMetaType(i), argv[i])));
            callHandler(handler, args, names);
        }));
    return true;
}

void EngineData::callHandler(const Handler &h, const Values &args, const QStringList &names) {
    try {
        if (h.node) {
            Env *e = rt->heap.make<Env>(h.scope);
            for (qsizetype i = 0; i < names.size(); ++i)
                e->vars.insert(names.at(i),
                               Env::Binding{QtJSPrivate::argument(args, static_cast<std::size_t>(i)), true, true});
            Function *f = QtJSPrivate::makeClosure(*rt, h.node, h.program, e);
            const Value r = rt->call(Value::object(f), h.thisObject, args);
            // "onClicked: function(mouse) { ... }" and "onClicked: (mouse) => ...": the value is the handler.
            if (h.node->exprBody && h.node->exprBody->kind == QtJSPrivate::Node::Kind::Function && rt->isCallable(r))
                rt->call(r, h.thisObject, args);
        } else if (h.function) {
            rt->call(Value::object(h.function), h.thisObject, args);
        }
    } catch (const Throw &t) {
        warn(QString(), errorText(t.value));
    }
    rt->maybeCollect();
}

// ---- Messages ----

QString EngineData::errorText(const Value &error) {
    try {
        QString text = rt->toString(error);
        if (error.isObject()) {
            const Value file = rt->getFromObject(error.asObject(), QStringLiteral("fileName"), error);
            const Value line = rt->getFromObject(error.asObject(), QStringLiteral("lineNumber"), error);
            if (file.isString() && line.isNumber())
                text = QStringLiteral("%1:%2: %3").arg(file.asString()).arg(line.asNumber()).arg(text);
        }
        return text;
    } catch (const Throw &) {
        return QStringLiteral("an error that cannot be shown");
    }
}

void EngineData::warn(const QQmlError &error) {
    emit engine->warnings({error});
    if (outputWarnings)
        qWarning().noquote() << error.toString();
}

void EngineData::warn(const QString &location, const QString &message) {
    QQmlError e;
    if (!location.isEmpty()) {
        const qsizetype colon = location.lastIndexOf(u':');
        e.setUrl(QUrl(location.left(colon)));
        e.setLine(location.mid(colon + 1).toInt());
    }
    e.setDescription(message);
    warn(e);
}

// ---- Documents ----

QUrl EngineData::resolve(const QUrl &base, const QUrl &relative) const {
    if (!relative.isRelative())
        return relative;
    if (base.isEmpty())
        return baseUrl.isEmpty() ? QUrl::fromLocalFile(QDir::currentPath() + u'/').resolved(relative)
                                 : baseUrl.resolved(relative);
    return base.resolved(relative);
}

std::shared_ptr<const ProgramData> EngineData::parse(const QByteArray &bytes, const QUrl &url,
                                                     QList<QQmlError> *errors) {
    QtJSPrivate::ParseError error;
    const QString name = url.toString();
    std::shared_ptr<ProgramData> program = QtJSPrivate::Parser::parseQml(QString::fromUtf8(bytes), name, &error);
    if (!program) {
        QQmlError e;
        e.setUrl(url);
        e.setLine(error.line);
        e.setColumn(error.column);
        e.setDescription(error.message);
        errors->append(e);
        return nullptr;
    }
    return program;
}

std::shared_ptr<const ProgramData> EngineData::load(const QUrl &url, QList<QQmlError> *errors) {
    const QString name = url.toString();
    if (const auto cached = documents.value(name))
        return cached;
    QString path;
    if (url.scheme() == QLatin1String("qrc"))
        path = u':' + url.path();
    else if (url.isLocalFile())
        path = url.toLocalFile();
    else if (url.isRelative())
        path = url.path();
    if (path.isEmpty()) {
        QQmlError e;
        e.setUrl(url);
        e.setDescription(QStringLiteral("only local files and resources can be loaded"));
        errors->append(e);
        return nullptr;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QQmlError e;
        e.setUrl(url);
        e.setDescription(QStringLiteral("cannot open %1: %2").arg(path, file.errorString()));
        errors->append(e);
        return nullptr;
    }
    const auto program = parse(file.readAll(), url, errors);
    if (program)
        documents.insert(name, program);
    return program;
}

// ---- The Qt object ----

void EngineData::installQtObject() {
    using QtJSPrivate::addMethod;
    using QtJSPrivate::argument;
    Object *qt = rt->newObject();
    rt->global->setOwn(QStringLiteral("Qt"), Value::object(qt), false);
    addMethod(*rt, qt, QStringLiteral("binding"), 1, [](Runtime &r, const Value &, const Values &args, Object *) {
        Function *f = argument(args, 0).asFunction();
        if (!f)
            r.throwTypeError(QStringLiteral("Qt.binding() needs a function"));
        Object *marker = r.newObject();
        marker->setOwn(QStringLiteral("__ntQmlBinding"), Value::object(f), false);
        return Value::object(marker);
    });
    addMethod(*rt, qt, QStringLiteral("callLater"), 1, [this](Runtime &r, const Value &, const Values &args, Object *) {
        Function *f = argument(args, 0).asFunction();
        if (!f)
            r.throwTypeError(QStringLiteral("Qt.callLater() needs a function"));
        if (std::find(m_callLater.begin(), m_callLater.end(), f) == m_callLater.end())
            m_callLater.push_back(f);
        if (!m_callLaterQueued) {
            m_callLaterQueued = true;
            QTimer::singleShot(0, engine, [this] {
                m_callLaterQueued = false;
                const std::vector<Function *> calls = std::exchange(m_callLater, {});
                for (Function *fn : calls) {
                    try {
                        rt->call(Value::object(fn), Value(), {});
                    } catch (const Throw &t) {
                        warn(QString(), errorText(t.value));
                    }
                }
                rt->maybeCollect();
            });
        }
        return Value();
    });
    addMethod(*rt, qt, QStringLiteral("quit"), 0, [this](Runtime &, const Value &, const Values &, Object *) {
        QTimer::singleShot(0, engine, [this] { emit engine->quit(); });
        return Value();
    });
    addMethod(*rt, qt, QStringLiteral("exit"), 1, [this](Runtime &r, const Value &, const Values &args, Object *) {
        const int code = static_cast<int>(r.toNumber(argument(args, 0)));
        QTimer::singleShot(0, engine, [this, code] { emit engine->exit(code); });
        return Value();
    });
    const auto shape = [&](const char *name, std::initializer_list<const char *> fields) {
        const QList<const char *> names(fields);
        addMethod(*rt, qt, QLatin1String(name), static_cast<int>(names.size()),
                  [names](Runtime &r, const Value &, const Values &args, Object *) {
                      Object *o = r.newObject();
                      for (qsizetype i = 0; i < names.size(); ++i)
                          o->setOwn(QLatin1String(names.at(i)),
                                    Value::number(r.toNumber(argument(args, static_cast<std::size_t>(i)))));
                      return Value::object(o);
                  });
    };
    shape("point", {"x", "y"});
    shape("size", {"width", "height"});
    shape("rect", {"x", "y", "width", "height"});
    const auto format = [&](const char *name, int kind) {
        addMethod(*rt, qt, QLatin1String(name), 2, [kind](Runtime &r, const Value &, const Values &args, Object *) {
            const QVariant v = r.toVariant(argument(args, 0));
            QDateTime dt = v.toDateTime();
            if (!dt.isValid())
                dt = QDateTime::fromString(v.toString(), Qt::ISODate);
            const Value f = argument(args, 1);
            if (f.isString()) {
                if (kind == 1)
                    return Value::string(dt.date().toString(f.asString()));
                if (kind == 2)
                    return Value::string(dt.time().toString(f.asString()));
                return Value::string(dt.toString(f.asString()));
            }
            if (kind == 1)
                return Value::string(dt.date().toString(Qt::ISODate));
            if (kind == 2)
                return Value::string(dt.time().toString(Qt::ISODate));
            return Value::string(dt.toString(Qt::ISODate));
        });
    };
    format("formatDate", 1);
    format("formatTime", 2);
    format("formatDateTime", 3);
    addMethod(*rt, qt, QStringLiteral("md5"), 1, [](Runtime &r, const Value &, const Values &args, Object *) {
        return Value::string(QString::fromLatin1(
            QCryptographicHash::hash(r.toString(argument(args, 0)).toUtf8(), QCryptographicHash::Md5).toHex()));
    });
    addMethod(*rt, qt, QStringLiteral("btoa"), 1, [](Runtime &r, const Value &, const Values &args, Object *) {
        return Value::string(QString::fromLatin1(r.toString(argument(args, 0)).toUtf8().toBase64()));
    });
    addMethod(*rt, qt, QStringLiteral("atob"), 1, [](Runtime &r, const Value &, const Values &args, Object *) {
        return Value::string(QString::fromUtf8(QByteArray::fromBase64(r.toString(argument(args, 0)).toLatin1())));
    });
    addMethod(*rt, qt, QStringLiteral("isQtObject"), 1, [](Runtime &r, const Value &, const Values &args, Object *) {
        return Value::boolean(r.unwrapQObject(argument(args, 0)) != nullptr);
    });
    addMethod(*rt, qt, QStringLiteral("resolvedUrl"), 1,
              [this](Runtime &r, const Value &, const Values &args, Object *) {
                  return Value::string(resolve(baseUrl, QUrl(r.toString(argument(args, 0)))).toString());
              });
    addMethod(*rt, qt, QStringLiteral("openUrlExternally"), 1,
              [](Runtime &, const Value &, const Values &, Object *) { return Value::boolean(false); });
    addMethod(*rt, qt, QStringLiteral("createComponent"), 1,
              [this](Runtime &r, const Value &, const Values &args, Object *) {
                  auto *c = new QQmlComponent(engine, resolve(baseUrl, QUrl(r.toString(argument(args, 0)))));
                  const Value v = r.wrapQObject(c);
                  r.scriptOwned.insert(c, true);
                  return v;
              });
    addMethod(*rt, qt, QStringLiteral("createQmlObject"), 3,
              [this](Runtime &r, const Value &, const Values &args, Object *) {
                  QQmlComponent component(engine);
                  const Value file = argument(args, 2);
                  component.setData(
                      r.toString(argument(args, 0)).toUtf8(),
                      resolve(baseUrl, QUrl(file.isUndefined() ? QStringLiteral("inline.qml") : r.toString(file))));
                  if (!component.isReady())
                      r.throwError(QtJSPrivate::ErrorType::Error,
                                   QStringLiteral("Qt.createQmlObject(): %1").arg(component.errorString()));
                  QObject *parent = r.unwrapQObject(argument(args, 1));
                  QObject *o = component.beginCreate(parent ? QQmlEngine::contextForObject(parent) : nullptr);
                  if (o && parent)
                      o->setParent(parent);
                  component.completeCreate();
                  if (!o)
                      r.throwError(QtJSPrivate::ErrorType::Error,
                                   QStringLiteral("Qt.createQmlObject(): %1").arg(component.errorString()));
                  return r.wrapQObject(o);
              });
    Object *platform = rt->newObject();
#if defined(Q_OS_ANDROID)
    platform->setOwn(QStringLiteral("os"), Value::string(QStringLiteral("android")));
#elif defined(Q_OS_WIN)
    platform->setOwn(QStringLiteral("os"), Value::string(QStringLiteral("windows")));
#elif defined(Q_OS_MACOS)
    platform->setOwn(QStringLiteral("os"), Value::string(QStringLiteral("osx")));
#else
    platform->setOwn(QStringLiteral("os"), Value::string(QStringLiteral("linux")));
#endif
    qt->setOwn(QStringLiteral("platform"), Value::object(platform));
    Object *application = rt->newObject();
    application->setOwn(QStringLiteral("name"), Value::string(QCoreApplication::applicationName()));
    application->setOwn(QStringLiteral("version"), Value::string(QCoreApplication::applicationVersion()));
    application->setOwn(QStringLiteral("organization"), Value::string(QCoreApplication::organizationName()));
    application->setOwn(QStringLiteral("domain"), Value::string(QCoreApplication::organizationDomain()));
    Values arguments;
    for (const QString &a : QCoreApplication::arguments())
        arguments.push_back(Value::string(a));
    application->setOwn(QStringLiteral("arguments"), Value::object(rt->newArray(arguments)));
    qt->setOwn(QStringLiteral("application"), Value::object(application));
}

} // namespace QtQmlPrivate

using QtQmlPrivate::EngineData;

// ================================================================================================================

/*!
    \class QQmlEngine
    \brief The QQmlEngine class makes objects from QML documents.

    An engine holds the types that QML documents can use, the documents it has read, and the root context
    in which components are made. Make objects with QQmlComponent, or use QQmlApplicationEngine, which
    loads a document and keeps its objects:

    \snippet qml/hello_qml.cpp component

    The engine is a QJSEngine whose global object has \c console, \c print(), \c qsTr() and the \c Qt
    object: \c{Qt.binding()}, \c{Qt.callLater()}, \c{Qt.quit()}, \c{Qt.point()}, \c{Qt.size()},
    \c{Qt.rect()}, \c{Qt.formatDate()}, \c{Qt.formatTime()}, \c{Qt.formatDateTime()},
    \c{Qt.createComponent()}, \c{Qt.createQmlObject()}, \c{Qt.resolvedUrl()}, \c{Qt.md5()},
    \c{Qt.btoa()}, \c{Qt.atob()}, \c{Qt.platform.os} and \c{Qt.application}.

    Errors in bindings and handlers do not stop the application: they are reported with warnings(), and
    printed unless setOutputWarningsToStandardError() turns that off.

    \differences
    Documents are interpreted, not compiled. There are no network URLs, image providers, URL interceptors,
    plugins or offline storage. QML types are registered with qmlRegisterType() and the other functions
    of \c{<qqml.h>}; the \c QML_ELEMENT macros compile but do nothing. retranslate() does nothing, as Nt
    has no translations.
    \enddifferences

    \sa QQmlComponent, QQmlContext, QQmlApplicationEngine, {Using Nt QML}
*/

/*!
    Constructs an engine with the given \a parent.
*/
QQmlEngine::QQmlEngine(QObject *parent) : QJSEngine(parent), m_data(std::make_unique<EngineData>(this)) {
    m_data->rootContext = new QQmlContext(this, this);
    installExtensions(AllExtensions);
}

/*!
    Destroys the engine. The objects made from QML that it still has must be deleted first.
*/
QQmlEngine::~QQmlEngine() {
    m_data->m_tearingDown = true;
    // Objects made by QML that outlive the engine keep working as plain QObjects.
    for (QObject *o : m_data->objects.keys())
        o->removeEventFilter(m_data->watcher);
    delete m_data->rootContext;
    m_data.reset();
}

/*!
    Returns the root context, the parent of the contexts that components are made in.
*/
QQmlContext *QQmlEngine::rootContext() const { return m_data->rootContext; }

/*!
    Forgets the documents that were read, so that they are read again when they are next used.
*/
void QQmlEngine::clearComponentCache() { m_data->documents.clear(); }

/*!
    Forgets the documents that were read; the same as clearComponentCache() in Nt.
*/
void QQmlEngine::trimComponentCache() { clearComponentCache(); }

/*!
    Returns the directories where modules are looked for.
*/
QStringList QQmlEngine::importPathList() const { return m_data->importPaths; }

/*!
    Sets the directories where modules are looked for to \a paths.
*/
void QQmlEngine::setImportPathList(const QStringList &paths) { m_data->importPaths = paths; }

/*!
    Adds \a dir to the directories where modules are looked for, before the others.
*/
void QQmlEngine::addImportPath(const QString &dir) {
    if (!m_data->importPaths.contains(dir))
        m_data->importPaths.prepend(dir);
}

/*!
    Returns the URL that relative URLs are resolved against, the current directory if it was not set.
*/
QUrl QQmlEngine::baseUrl() const {
    return m_data->baseUrl.isEmpty() ? QUrl::fromLocalFile(QDir::currentPath() + u'/') : m_data->baseUrl;
}

/*!
    Sets the URL that relative URLs are resolved against to \a url.
*/
void QQmlEngine::setBaseUrl(const QUrl &url) { m_data->baseUrl = url; }

/*!
    Returns \a url; Nt has no URL interceptors. \a type is not used.
*/
QUrl QQmlEngine::interceptUrl(const QUrl &url, int type) const {
    (void)type;
    return url;
}

/*!
    Returns \c true if warnings are printed as well as emitted with warnings(); otherwise returns \c false.
*/
bool QQmlEngine::outputWarningsToStandardError() const { return m_data->outputWarnings; }

/*!
    Prints warnings if \a enabled is \c true, the default.
*/
void QQmlEngine::setOutputWarningsToStandardError(bool enabled) { m_data->outputWarnings = enabled; }

/*!
    Returns the directory for offline storage. Nt has none, so it is empty unless set.
*/
QString QQmlEngine::offlineStoragePath() const { return property("_q_ntOfflineStorage").toString(); }

/*!
    Sets the directory for offline storage to \a dir. Nt keeps it but stores nothing.
*/
void QQmlEngine::setOfflineStoragePath(const QString &dir) { setProperty("_q_ntOfflineStorage", dir); }

/*!
    \fn template <typename T> T QQmlEngine::singletonInstance(const QString &moduleName, const QString &typeName)

    Returns the instance of the singleton \a typeName of \a moduleName, cast to \c T, or \c nullptr.
*/

QObject *QtQmlPrivate::EngineData::singleton(const QString &module, const QString &name) {
    return engine->singleton(module, name);
}

QObject *QQmlEngine::singleton(const QString &moduleName, const QString &typeName) {
    const QtQmlPrivate::TypeInfo *t = QtQmlPrivate::TypeRegistry::instance().find(moduleName, typeName);
    if (!t || !t->singleton)
        return nullptr;
    auto *type = const_cast<QtQmlPrivate::TypeInfo *>(t);
    if (!type->singletonInstance && type->singletonFactory) {
        type->singletonInstance = type->singletonFactory(this, this);
        if (type->singletonInstance && !type->singletonInstance->parent())
            type->singletonInstance->setParent(this);
    }
    return type->singletonInstance.data();
}

/*!
    Returns the context that \a object was made in, or \c nullptr if QML did not make it.
*/
QQmlContext *QQmlEngine::contextForObject(const QObject *object) {
    QMutexLocker lock(&QtQmlPrivate::g_contextsMutex);
    return QtQmlPrivate::contexts().value(object).data();
}

/*!
    Sets the context of \a object to \a context.
*/
void QQmlEngine::setContextForObject(QObject *object, QQmlContext *context) {
    if (!object)
        return;
    QMutexLocker lock(&QtQmlPrivate::g_contextsMutex);
    QtQmlPrivate::contexts().insert(object, context);
}

/*!
    Does nothing: Nt has no translations.
*/
void QQmlEngine::retranslate() {}

// Returns the engine of the context that object was made in, or nullptr.
QQmlEngine *qmlEngine(const QObject *object) {
    QQmlContext *c = QQmlEngine::contextForObject(object);
    return c ? c->engine() : nullptr;
}

// Returns the context that object was made in, or nullptr.
QQmlContext *qmlContext(const QObject *object) { return QQmlEngine::contextForObject(object); }

// ================================================================================================================

/*!
    \class QQmlContext
    \brief The QQmlContext class is a scope of names for the expressions of a QML component.

    Each component made by an engine has a context, whose parent is the context it was made in, up to the
    root context of the engine. Expressions see the ids of their component, and the context properties
    and the properties of the context object of their context and its parents.

    Context properties give documents values from C++:

    \code
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("version", "1.4");
    \endcode

    Bindings that read a context property are evaluated again when setContextProperty() changes it.

    \differences
    None.
    \enddifferences

    \sa QQmlEngine::rootContext(), QQmlComponent::create()
*/

/*!
    Constructs a context of \a engine whose parent is the root context of the engine, with the given QObject
    \a parent.
*/
QQmlContext::QQmlContext(QQmlEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine), m_parent(engine ? engine->rootContext() : nullptr), m_parentGuard(m_parent) {}

/*!
    Constructs a context whose parent is \a parentContext, with the given QObject \a parent.
*/
QQmlContext::QQmlContext(QQmlContext *parentContext, QObject *parent)
    : QObject(parent), m_engine(parentContext ? parentContext->engine() : nullptr), m_parent(parentContext),
      m_parentGuard(parentContext) {}

/*!
    Destroys the context. Expressions made in it can no longer see its names.
*/
QQmlContext::~QQmlContext() = default;

/*!
    Returns \c true if the context belongs to an engine; otherwise returns \c false.
*/
bool QQmlContext::isValid() const { return m_engine != nullptr; }

/*!
    Returns the engine of the context.
*/
QQmlEngine *QQmlContext::engine() const { return m_engine; }

/*!
    Returns the parent context, or \c nullptr for a root context.
*/
QQmlContext *QQmlContext::parentContext() const { return m_parentGuard ? m_parent : nullptr; }

/*!
    Returns the context object, whose properties expressions see as names.
*/
QObject *QQmlContext::contextObject() const { return m_contextObject.data(); }

/*!
    Sets the context object to \a object.
*/
void QQmlContext::setContextObject(QObject *object) { m_contextObject = object; }

/*!
    Returns the context property \a name, or the property of the context object of that name, looking in
    the parent contexts if this one has neither.
*/
QVariant QQmlContext::contextProperty(const QString &name) const {
    for (const QQmlContext *c = this; c; c = c->parentContext()) {
        if (c->m_properties.contains(name))
            return c->m_properties.value(name);
        if (QObject *o = c->m_ids.value(name).data())
            return QVariant::fromValue(o);
        if (c->m_contextObject && c->m_contextObject->metaObject()->indexOfProperty(name.toUtf8().constData()) >= 0)
            return c->m_contextObject->property(name.toUtf8().constData());
    }
    return QVariant();
}

/*!
    \overload

    Sets the context property \a name to the object \a value.
*/
void QQmlContext::setContextProperty(const QString &name, QObject *value) {
    setContextProperty(name, QVariant::fromValue(value));
}

/*!
    Sets the context property \a name to \a value. Bindings that read it are evaluated again.
*/
void QQmlContext::setContextProperty(const QString &name, const QVariant &value) {
    m_properties.insert(name, value);
    if (EngineData *d = EngineData::of(m_engine))
        d->changed(EngineData::key(this, name));
}

/*!
    Sets the context properties in \a properties.
*/
void QQmlContext::setContextProperties(const QList<PropertyPair> &properties) {
    for (const PropertyPair &p : properties)
        setContextProperty(p.name, p.value);
}

/*!
    Returns the id of \a object in this context, or an empty string.
*/
QString QQmlContext::nameForObject(const QObject *object) const {
    for (auto it = m_ids.cbegin(); it != m_ids.cend(); ++it)
        if (it.value() == object)
            return it.key();
    for (auto it = m_properties.cbegin(); it != m_properties.cend(); ++it) {
        const QVariant &v = it.value();
        if (object && (v.metaType().flags() & QMetaType::PointerToQObject) &&
            *static_cast<QObject *const *>(v.constData()) == object)
            return it.key();
    }
    return QString();
}

/*!
    Returns the object with the id \a name in this context, or \c nullptr.
*/
QObject *QQmlContext::objectForName(const QString &name) const { return m_ids.value(name).data(); }

/*!
    Returns \a url resolved against the base URL of the context, or of its parents.
*/
QUrl QQmlContext::resolvedUrl(const QUrl &url) const {
    if (!url.isRelative())
        return url;
    for (const QQmlContext *c = this; c; c = c->parentContext())
        if (!c->m_baseUrl.isEmpty())
            return c->m_baseUrl.resolved(url);
    return m_engine ? m_engine->baseUrl().resolved(url) : url;
}

/*!
    Sets the base URL of the context to \a baseUrl.
*/
void QQmlContext::setBaseUrl(const QUrl &baseUrl) { m_baseUrl = baseUrl; }

/*!
    Returns the base URL of the context, or of its parents.
*/
QUrl QQmlContext::baseUrl() const {
    for (const QQmlContext *c = this; c; c = c->parentContext())
        if (!c->m_baseUrl.isEmpty())
            return c->m_baseUrl;
    return QUrl();
}

// ================================================================================================================

/*!
    \class QQmlError
    \brief The QQmlError class describes an error in a QML document.

    \code
    for (const QQmlError &e : component.errors())
        qWarning() << e.url() << e.line() << e.description();
    \endcode

    \differences
    There is no \c object() or \c swap().
    \enddifferences

    \sa QQmlComponent::errors(), QQmlEngine::warnings()
*/

/*!
    \fn QQmlError::QQmlError()

    Constructs an empty error.
*/

/*!
    Returns \c true if the error has a URL and a description; otherwise returns \c false.
*/
bool QQmlError::isValid() const { return !m_url.isEmpty() || !m_description.isEmpty(); }

/*!
    Returns the URL of the document.
*/
QUrl QQmlError::url() const { return m_url; }

/*!
    Sets the URL of the document to \a url.
*/
void QQmlError::setUrl(const QUrl &url) { m_url = url; }

/*!
    Returns the description.
*/
QString QQmlError::description() const { return m_description; }

/*!
    Sets the description to \a description.
*/
void QQmlError::setDescription(const QString &description) { m_description = description; }

/*!
    Returns the line, from 1, or -1 if it is not known.
*/
int QQmlError::line() const { return m_line; }

/*!
    Sets the line to \a line.
*/
void QQmlError::setLine(int line) { m_line = line; }

/*!
    Returns the column, from 1, or -1 if it is not known.
*/
int QQmlError::column() const { return m_column; }

/*!
    Sets the column to \a column.
*/
void QQmlError::setColumn(int column) { m_column = column; }

/*!
    Returns the type of the message, QtWarningMsg by default.
*/
QtMsgType QQmlError::messageType() const { return m_type; }

/*!
    Sets the type of the message to \a messageType.
*/
void QQmlError::setMessageType(QtMsgType messageType) { m_type = messageType; }

/*!
    Returns the error as "url:line:column: description".
*/
QString QQmlError::toString() const {
    QString s = m_url.isEmpty() ? QStringLiteral("<Unknown File>") : m_url.toString();
    if (m_line > 0) {
        s += u':' + QString::number(m_line);
        if (m_column > 0)
            s += u':' + QString::number(m_column);
    }
    return s + QStringLiteral(": ") + m_description;
}

// Writes error to debug and returns debug.
QDebug operator<<(QDebug debug, const QQmlError &error) {
    QDebugStateSaver saver(debug);
    debug.noquote() << error.toString();
    return debug;
}

/*!
    \class QQmlParserStatus
    \brief The QQmlParserStatus class tells an object when QML starts and finishes making it.

    A C++ type that needs all its properties before it can do its work inherits QQmlParserStatus as well
    as QObject: classBegin() is called when the object has been made, before its properties are set, and
    componentComplete() when its component is complete, all bindings set.

    \code
    class Loader : public QObject, public QQmlParserStatus {
        Q_OBJECT
        Q_INTERFACES(QQmlParserStatus)
        ...
        void classBegin() override {}
        void componentComplete() override { load(m_source); }
    };
    \endcode

    \differences
    None; \c Q_INTERFACES is not needed in Nt, which finds the interface with \c dynamic_cast.
    \enddifferences
*/

/*!
    Destroys the object.
*/
QQmlParserStatus::~QQmlParserStatus() = default;

/*!
    \fn void QQmlParserStatus::classBegin()

    Called when the object has been made, before its properties are set.
*/

/*!
    \fn void QQmlParserStatus::componentComplete()

    Called when the component of the object is complete: all its objects made and their bindings set.
*/

#include "moc_qqmlcontext.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qqmlengine.cpp"  // NOLINT(bugprone-suspicious-include): generated by ntmoc
