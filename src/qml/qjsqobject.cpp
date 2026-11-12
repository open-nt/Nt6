// MIT License - see LICENSE
//
// QObjects and C++ values in the JavaScript runtime: a QObject is seen through a wrapper whose properties are
// its Q_PROPERTYs, and whose functions are its slots, invokable methods and signals. Signals can be connected
// to script functions with signal.connect(f).
#include "qjsbuiltins_p.h"

#include <QtCore/qdatetime.h>
#include <QtQml/qjsengine.h>
#include <QtCore/qline.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qurl.h>

#include <cmath>

namespace QtJSPrivate {

namespace {

QList<QMetaMethod> methodsNamed(const QMetaObject *mo, const QString &name) {
    QList<QMetaMethod> found;
    const QByteArray n = name.toUtf8();
    for (int i = mo->methodCount() - 1; i >= 0; --i) {
        const QMetaMethod m = mo->method(i);
        if (m.name() != n || m.access() == QMetaMethod::Private)
            continue;
        if (m.methodType() == QMetaMethod::Constructor)
            continue;
        // A subclass's method of the same signature hides the base's.
        bool hidden = false;
        for (const QMetaMethod &f : std::as_const(found))
            hidden = hidden || f.methodSignature() == m.methodSignature();
        if (!hidden)
            found.append(m);
    }
    return found;
}

// How well a value fits a parameter type: higher is better, negative is no fit.
int fit(const Value &v, QMetaType type) {
    const int id = type.id();
    if (id == QMetaType::QVariant)
        return 1;
    switch (v.type()) {
    case Value::Number:
        if (id == QMetaType::Double || id == QMetaType::Float)
            return 3;
        if (id == QMetaType::Int || id == QMetaType::UInt || id == QMetaType::LongLong || id == QMetaType::ULongLong ||
            id == QMetaType::Long || id == QMetaType::ULong || id == QMetaType::Short || id == QMetaType::UShort ||
            (type.flags() & QMetaType::IsEnumeration))
            return v.asNumber() == std::trunc(v.asNumber()) ? 3 : 2;
        return id == QMetaType::QString || id == QMetaType::Bool ? 1 : 0;
    case Value::String:
        return id == QMetaType::QString ? 3 : id == QMetaType::QUrl || id == QMetaType::QByteArray ? 2 : 0;
    case Value::Boolean:
        return id == QMetaType::Bool ? 3 : 0;
    case Value::ObjectType:
        if (type.flags() & QMetaType::PointerToQObject)
            return 2;
        return id == QMetaType::QVariantList || id == QMetaType::QVariantMap || id == QMetaType::QStringList ? 2 : 0;
    default:
        return 0;
    }
}

Value invokeMethod(Runtime &runtime, QObject *object, const QList<QMetaMethod> &overloads, const Values &args,
                   const QString &name) {
    if (!object)
        runtime.throwTypeError(QStringLiteral("cannot call %1: the object was deleted").arg(name));
    const QMetaMethod *best = nullptr;
    int bestScore = -1;
    for (const QMetaMethod &m : overloads) {
        const int count = m.parameterCount();
        if (count > static_cast<int>(args.size()) || count > 10)
            continue;
        int score = count * 100;
        for (int i = 0; i < count; ++i)
            score += fit(args[static_cast<std::size_t>(i)], m.parameterMetaType(i));
        if (score > bestScore) {
            bestScore = score;
            best = &m;
        }
    }
    if (!best)
        runtime.throwTypeError(QStringLiteral("%1(): too few arguments").arg(name));
    const QMetaMethod m = *best;
    const int count = m.parameterCount();
    const QList<QByteArray> typeNames = m.parameterTypes();
    QVariant values[10];
    QGenericArgument a[10];
    for (int i = 0; i < count; ++i) {
        const QMetaType type = m.parameterMetaType(i);
        const Value &arg = args[static_cast<std::size_t>(i)];
        if (type.id() == QMetaType::QVariant) {
            values[i] = QVariant::fromValue(runtime.toVariant(arg));
        } else {
            values[i] = runtime.toVariant(arg, type);
            if (values[i].metaType() != type && !values[i].convert(type))
                runtime.throwTypeError(QStringLiteral("%1(): argument %2 cannot be converted to %3")
                                           .arg(name)
                                           .arg(i + 1)
                                           .arg(QString::fromUtf8(typeNames.value(i))));
        }
        a[i] = QGenericArgument(typeNames.at(i).constData(), values[i].constData());
    }
    const QMetaType returnType = m.returnMetaType();
    QVariant result;
    QGenericReturnArgument r;
    QByteArray returnName;
    if (returnType.isValid() && returnType.id() != QMetaType::Void) {
        result = QVariant(returnType);
        returnName = m.typeName();
        r = QGenericReturnArgument(returnName.constData(), result.data());
    }
    if (!m.invoke(object, Qt::DirectConnection, r, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]))
        runtime.throwTypeError(QStringLiteral("%1() could not be called").arg(name));
    if (runtime.hasPendingError) {
        // The method called QJSEngine::throwError().
        runtime.hasPendingError = false;
        const Value error = runtime.pendingError;
        runtime.pendingError = Value();
        throw Throw{error};
    }
    if (!result.isValid())
        return Value();
    if (returnType.flags() & QMetaType::PointerToQObject) {
        // As in Qt, a QObject without a parent returned to a script belongs to the script engine,
        // unless its ownership was set: the engine deletes it once no script can reach it.
        QObject *o = *static_cast<QObject *const *>(result.constData());
        if (o && !o->parent()) {
            const QVariant explicitOwnership = o->property("_q_ntJSOwnership");
            if (!explicitOwnership.isValid() || explicitOwnership.toInt() == QJSEngine::JavaScriptOwnership)
                runtime.scriptOwned.insert(o, true);
        }
    }
    if (returnType.id() == QMetaType::QVariant)
        return runtime.fromVariant(*static_cast<const QVariant *>(result.constData()));
    return runtime.fromVariant(result);
}

Value pointObject(Runtime &runtime, std::initializer_list<std::pair<const char *, double>> fields) {
    Object *o = runtime.newObject();
    for (const auto &f : fields)
        o->setOwn(QLatin1String(f.first), Value::number(f.second));
    return Value::object(o);
}

double field(Runtime &runtime, Object *o, const char *name) {
    return runtime.toNumber(runtime.getFromObject(o, QLatin1String(name), Value::object(o)));
}

} // namespace

QObjectWrapper::QObjectWrapper(Object *prototype, QObject *o) : Object(prototype, Class::Host), object(o), key(o) {}

bool QObjectWrapper::getOwn(Runtime &runtime, const QString &name, const Value &receiver, Value *out) {
    QObject *o = object.data();
    if (o) {
        const QMetaObject *mo = o->metaObject();
        const int p = mo->indexOfProperty(name.toUtf8().constData());
        if (p >= 0) {
            const QMetaProperty prop = mo->property(p);
            if (runtime.qmlRead)
                runtime.qmlRead(o, name, p);
            const QVariant v = prop.read(o);
            *out = prop.isEnumType() || prop.isFlagType() ? Value::number(v.toInt()) : runtime.fromVariant(v);
            return true;
        }
    }
    if (o && runtime.qmlGet && runtime.qmlGet(o, name, out))
        return true;
    if (Object::getOwn(runtime, name, receiver, out))
        return true;
    if (!o)
        return false;
    if (Function *f = methods.value(name)) {
        *out = Value::object(f);
        return true;
    }
    const QList<QMetaMethod> overloads = methodsNamed(o->metaObject(), name);
    if (overloads.isEmpty()) {
        // The keys of the object's Q_ENUMs, as Qt gives them on singletons: Singleton.Key.
        const QMetaObject *mo = o->metaObject();
        const QByteArray enumKey = name.toUtf8();
        for (int i = 0; i < mo->enumeratorCount(); ++i) {
            bool ok = false;
            const int value = mo->enumerator(i).keyToValue(enumKey.constData(), &ok);
            if (ok) {
                *out = Value::number(value);
                return true;
            }
        }
        return false;
    }
    QPointer<QObject> guard = o;
    Function *f = runtime.newNative(name, overloads.first().parameterCount(),
                                    [guard, overloads, name](Runtime &rt, const Value &, const Values &args, Object *) {
                                        return invokeMethod(rt, guard.data(), overloads, args, name);
                                    });
    const QMetaMethod first = overloads.first();
    if (first.methodType() == QMetaMethod::Signal) {
        // signal.connect(f), signal.connect(thisObject, f) and signal.disconnect(...).
        const int index = first.methodIndex();
        addMethod(
            runtime, f, QStringLiteral("connect"), 1,
            [guard, index](Runtime &rt, const Value &, const Values &args, Object *) {
                QObject *sender = guard.data();
                if (!sender)
                    rt.throwTypeError(QStringLiteral("cannot connect: the object was deleted"));
                const Value self = args.size() >= 2 ? args[0] : Value();
                const Value fn = args.size() >= 2 ? args[1] : argument(args, 0);
                Function *target = fn.asFunction();
                if (!target)
                    rt.throwTypeError(QStringLiteral("connect() needs a function"));
                const QMetaMethod signal = sender->metaObject()->method(index);
                Runtime::ScriptConnection c;
                c.sender = sender;
                c.signalIndex = index;
                c.thisObject = self;
                c.function = target;
                Runtime *engineRuntime = &rt;
                c.connection = QtPrivate::Access::connectFunction(
                    sender, signal, rt.connectionContext, [engineRuntime, signal, target, self](void *const *argv) {
                        Values call;
                        for (int i = 0; i < signal.parameterCount(); ++i)
                            call.push_back(engineRuntime->fromVariant(QVariant(signal.parameterMetaType(i), argv[i])));
                        try {
                            engineRuntime->call(Value::object(target), self, call);
                        } catch (const Throw &t) {
                            if (engineRuntime->reportError)
                                engineRuntime->reportError(t.value);
                        }
                        engineRuntime->maybeCollect();
                    });
                rt.connections.push_back(c);
                return Value();
            });
        addMethod(runtime, f, QStringLiteral("disconnect"), 1,
                  [guard, index](Runtime &rt, const Value &, const Values &args, Object *) {
                      const Value self = args.size() >= 2 ? args[0] : Value();
                      const Value fn = args.size() >= 2 ? args[1] : argument(args, 0);
                      for (auto it = rt.connections.begin(); it != rt.connections.end(); ++it) {
                          if (it->sender != guard || it->signalIndex != index || it->function != fn.asFunction() ||
                              !rt.strictEquals(it->thisObject, self))
                              continue;
                          QObject::disconnect(it->connection);
                          rt.connections.erase(it);
                          return Value::boolean(true);
                      }
                      return Value::boolean(false);
                  });
    }
    methods.insert(name, f);
    *out = Value::object(f);
    return true;
}

bool QObjectWrapper::setOwn(Runtime &runtime, const QString &name, const Value &value, const Value &receiver) {
    QObject *o = object.data();
    if (o) {
        const QMetaObject *mo = o->metaObject();
        const int p = mo->indexOfProperty(name.toUtf8().constData());
        if (p >= 0) {
            const QMetaProperty prop = mo->property(p);
            if (!prop.isWritable())
                runtime.throwTypeError(QStringLiteral("cannot assign to the read-only property '%1'").arg(name));
            if (runtime.qmlWrite)
                runtime.qmlWrite(o, name);
            QVariant v;
            if ((prop.isEnumType() || prop.isFlagType()) && value.isString()) {
                bool ok = false;
                const QByteArray keys = value.asString().toUtf8();
                const int e = prop.isFlagType() ? prop.enumerator().keysToValue(keys.constData(), &ok)
                                                : prop.enumerator().keyToValue(keys.constData(), &ok);
                if (!ok)
                    runtime.throwTypeError(QStringLiteral("'%1' is not a value of %2").arg(value.asString(), name));
                v = QVariant(e);
            } else {
                v = runtime.toVariant(value, prop.metaType());
            }
            if (!prop.write(o, v))
                runtime.throwTypeError(QStringLiteral("cannot assign %1 to the property '%2' of type %3")
                                           .arg(runtime.toString(value), name, QString::fromLatin1(prop.typeName())));
            return true;
        }
        if (runtime.qmlSet && runtime.qmlSet(o, name, value))
            return true;
        if (!methodsNamed(mo, name).isEmpty() && !ownProperty(name))
            runtime.throwTypeError(QStringLiteral("cannot assign to the method '%1'").arg(name));
    }
    return Object::setOwn(runtime, name, value, receiver);
}

bool QObjectWrapper::hasOwn(Runtime &runtime, const QString &name) {
    if (QObject *o = object.data()) {
        const QMetaObject *mo = o->metaObject();
        if (mo->indexOfProperty(name.toUtf8().constData()) >= 0 || !methodsNamed(mo, name).isEmpty())
            return true;
        if (runtime.qmlHas && runtime.qmlHas(o, name))
            return true;
    }
    return Object::hasOwn(runtime, name);
}

bool QObjectWrapper::deleteOwn(Runtime &runtime, const QString &name) {
    if (QObject *o = object.data())
        if (o->metaObject()->indexOfProperty(name.toUtf8().constData()) >= 0)
            return false;
    return Object::deleteOwn(runtime, name);
}

QStringList QObjectWrapper::enumerableKeys(Runtime &runtime) {
    QStringList keys;
    if (QObject *o = object.data()) {
        const QMetaObject *mo = o->metaObject();
        for (int i = 0; i < mo->propertyCount(); ++i)
            keys.append(QString::fromLatin1(mo->property(i).name()));
    }
    for (const QString &k : Object::enumerableKeys(runtime))
        if (!keys.contains(k))
            keys.append(k);
    return keys;
}

void QObjectWrapper::markChildren(std::vector<Cell *> *stack) const {
    Object::markChildren(stack);
    for (Function *f : methods)
        if (!f->marked)
            stack->push_back(f);
}

void Runtime::installQObjectSupport() {
    qobjectProto = newObject();
    addMethod(*this, qobjectProto, QStringLiteral("toString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  QObject *o = rt.unwrapQObject(self);
                  if (!o)
                      return Value::string(QStringLiteral("null"));
                  const QString name = o->objectName();
                  return Value::string(QStringLiteral("%1(0x%2%3)")
                                           .arg(QString::fromLatin1(o->metaObject()->className()))
                                           .arg(reinterpret_cast<quintptr>(o), 0, 16)
                                           .arg(name.isEmpty() ? QString() : QStringLiteral(", \"%1\"").arg(name)));
              });
}

Value Runtime::wrapQObject(QObject *object) {
    if (!object)
        return Value::null();
    if (Object *w = wrappers.value(object)) {
        auto *wrapper = static_cast<QObjectWrapper *>(w);
        if (wrapper->object.data() == object)
            return Value::object(w);
    }
    auto *w = heap.make<QObjectWrapper>(qobjectProto, object);
    wrappers.insert(object, w);
    // The entry goes when the object does, so that a new object at the same address gets a new wrapper.
    QObject::connect(object, &QObject::destroyed, connectionContext, [this, object, w] {
        if (wrappers.value(object) == w) {
            wrappers.remove(object);
            scriptOwned.remove(object);
        }
    });
    return Value::object(w);
}

QObject *Runtime::unwrapQObject(const Value &value) const {
    if (!value.isObject())
        return nullptr;
    auto *w = dynamic_cast<QObjectWrapper *>(value.asObject());
    return w ? w->object.data() : nullptr;
}

Value Runtime::fromVariant(const QVariant &variant) {
    if (!variant.isValid())
        return Value();
    if (fromVariantHook) {
        Value out;
        if (fromVariantHook(variant, &out))
            return out;
    }
    const QMetaType type = variant.metaType();
    if (type.flags() & QMetaType::PointerToQObject)
        return wrapQObject(*static_cast<QObject *const *>(variant.constData()));
    if (type.flags() & QMetaType::IsEnumeration)
        return Value::number(variant.toInt());
    switch (type.id()) {
    case QMetaType::Nullptr:
        return Value::null();
    case QMetaType::Bool:
        return Value::boolean(variant.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Long:
    case QMetaType::Short:
    case QMetaType::ULong:
    case QMetaType::UShort:
    case QMetaType::UChar:
    case QMetaType::SChar:
    case QMetaType::Float:
        return Value::number(variant.toDouble());
    case QMetaType::Char:
    case QMetaType::QChar:
    case QMetaType::QString:
    case QMetaType::QByteArray:
    case QMetaType::QUrl:
        return Value::string(variant.toString());
    case QMetaType::QStringList: {
        Values items;
        for (const QString &s : variant.toStringList())
            items.push_back(Value::string(s));
        return Value::object(newArray(items));
    }
    case QMetaType::QVariantList: {
        Values items;
        for (const QVariant &v : variant.toList())
            items.push_back(fromVariant(v));
        return Value::object(newArray(items));
    }
    case QMetaType::QVariantMap: {
        Object *o = newObject();
        const QVariantMap map = variant.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            o->setOwn(it.key(), fromVariant(it.value()));
        return Value::object(o);
    }
    case QMetaType::QVariantHash: {
        Object *o = newObject();
        const QVariantHash hash = variant.toHash();
        for (auto it = hash.cbegin(); it != hash.cend(); ++it)
            o->setOwn(it.key(), fromVariant(it.value()));
        return Value::object(o);
    }
    case QMetaType::QDateTime:
    case QMetaType::QDate: {
        const QDateTime dt =
            type.id() == QMetaType::QDate ? QDateTime(variant.toDate(), QTime(0, 0), Qt::UTC) : variant.toDateTime();
        Object *d = heap.make<Object>(dateProto, Object::Class::Date);
        d->primitive = Value::number(dt.isValid() ? static_cast<double>(dt.toMSecsSinceEpoch())
                                                  : std::numeric_limits<double>::quiet_NaN());
        return Value::object(d);
    }
    case QMetaType::QTime:
        return Value::string(variant.toTime().toString(Qt::ISODateWithMs));
    case QMetaType::QPoint:
    case QMetaType::QPointF: {
        const QPointF p = variant.toPointF();
        return pointObject(*this, {{"x", p.x()}, {"y", p.y()}});
    }
    case QMetaType::QSize:
    case QMetaType::QSizeF: {
        const QSizeF s = variant.toSizeF();
        return pointObject(*this, {{"width", s.width()}, {"height", s.height()}});
    }
    case QMetaType::QRect:
    case QMetaType::QRectF: {
        const QRectF r = variant.toRectF();
        return pointObject(*this, {{"x", r.x()}, {"y", r.y()}, {"width", r.width()}, {"height", r.height()}});
    }
    case QMetaType::QVariant:
        return fromVariant(*static_cast<const QVariant *>(variant.constData()));
    default:
        break;
    }
    // A value without a JavaScript form is carried as it is.
    Object *o = heap.make<Object>(objectProto, Object::Class::Host);
    o->variant = variant;
    addMethod(*this, o, QStringLiteral("toString"), 0, [variant](Runtime &, const Value &, const Values &, Object *) {
        const QString s = variant.toString();
        return Value::string(s.isEmpty() ? QStringLiteral("QVariant(%1)").arg(QString::fromLatin1(variant.typeName()))
                                         : s);
    });
    return Value::object(o);
}

QVariant Runtime::toVariant(const Value &value, QMetaType target) {
    if (target.isValid() && toVariantHook) {
        QVariant out;
        if (toVariantHook(value, target, &out))
            return out;
    }
    const int id = target.isValid() ? target.id() : QMetaType::UnknownType;
    if (id == QMetaType::QVariant)
        return toVariant(value);
    if (target.isValid() && (target.flags() & QMetaType::PointerToQObject)) {
        QObject *o = unwrapQObject(value);
        if (o && target.metaObject() && !o->metaObject()->inherits(target.metaObject()))
            o = nullptr;
        return QVariant(target, static_cast<const void *>(&o));
    }
    if (value.isObject()) {
        Object *o = value.asObject();
        switch (id) {
        case QMetaType::QPoint:
        case QMetaType::QPointF: {
            const QPointF p(field(*this, o, "x"), field(*this, o, "y"));
            return id == QMetaType::QPoint ? QVariant(p.toPoint()) : QVariant(p);
        }
        case QMetaType::QSize:
        case QMetaType::QSizeF: {
            const QSizeF s(field(*this, o, "width"), field(*this, o, "height"));
            return id == QMetaType::QSize ? QVariant(s.toSize()) : QVariant(s);
        }
        case QMetaType::QRect:
        case QMetaType::QRectF: {
            const QRectF r(field(*this, o, "x"), field(*this, o, "y"), field(*this, o, "width"),
                           field(*this, o, "height"));
            return id == QMetaType::QRect ? QVariant(r.toRect()) : QVariant(r);
        }
        case QMetaType::QStringList: {
            QStringList list;
            if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments)
                for (const Value &v : o->elements)
                    list.append(toString(v));
            return list;
        }
        case QMetaType::QString:
            return toString(value);
        default:
            break;
        }
    }
    QVariant v;
    switch (value.type()) {
    case Value::Undefined:
        v = QVariant();
        break;
    case Value::Null:
        v = QVariant::fromValue(nullptr);
        break;
    case Value::Boolean:
        v = value.asBoolean();
        break;
    case Value::Number: {
        const double d = value.asNumber();
        if (id == QMetaType::Int || id == QMetaType::Long || id == QMetaType::Short ||
            (target.isValid() && (target.flags() & QMetaType::IsEnumeration))) {
            // Numbers out of range become 0, as a C++ cast must not be relied on.
            const double t = std::trunc(d);
            v = std::isfinite(t) && t >= -2147483648.0 && t <= 2147483647.0 ? static_cast<int>(t) : 0;
        } else if (id == QMetaType::UInt) {
            const double t = std::trunc(d);
            v = std::isfinite(t) && t >= 0 && t <= 4294967295.0 ? static_cast<uint>(t) : 0u;
        } else if (id == QMetaType::LongLong) {
            const double t = std::trunc(d);
            v = std::isfinite(t) && std::fabs(t) < 9.2e18 ? static_cast<qlonglong>(t) : 0LL;
        } else if (id == QMetaType::Bool) {
            v = toBoolean(value);
        } else if (id == QMetaType::QString) {
            v = toString(value);
        } else {
            v = d;
        }
        break;
    }
    case Value::String:
        v = value.asString();
        break;
    case Value::ObjectType: {
        Object *o = value.asObject();
        if (QObject *q = unwrapQObject(value)) {
            v = QVariant::fromValue(q);
        } else if (o->cls == Object::Class::Host && o->variant.isValid()) {
            v = o->variant;
        } else if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments) {
            QVariantList list;
            for (const Value &e : o->elements)
                list.append(toVariant(e));
            v = list;
        } else if (o->cls == Object::Class::Date) {
            const double t = o->primitive.asNumber();
            v = std::isnan(t) ? QDateTime() : QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t));
        } else if (o->cls == Object::Class::Number || o->cls == Object::Class::String ||
                   o->cls == Object::Class::Boolean) {
            return toVariant(o->primitive, target);
        } else if (value.asFunction()) {
            v = QVariant();
        } else {
            QVariantMap map;
            for (const QString &k : o->enumerableKeys(*this))
                map.insert(k, toVariant(getFromObject(o, k, value)));
            v = map;
        }
        break;
    }
    }
    if (target.isValid() && v.isValid() && v.metaType() != target) {
        QVariant converted = v;
        if (converted.convert(target))
            return converted;
    }
    return v;
}

} // namespace QtJSPrivate
