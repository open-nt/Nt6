// MIT License - see LICENSE
//
// The built-in objects of the JavaScript runtime: Object, Function, Array, Boolean, the errors, Math and the
// global functions. Strings, regular expressions, numbers, dates and JSON are in files of their own.
#include "qjsbuiltins_p.h"

#include "qjsinterp_p.h"
#include "qjsparser_p.h"

#include <QtCore/qrandom.h>
#include <QtCore/qurl.h>

#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>

namespace QtJSPrivate {

Function *addMethod(Runtime &runtime, Object *object, const QString &name, int length, Function::Native fn) {
    Function *f = runtime.newNative(name, length, std::move(fn));
    object->setOwn(name, Value::object(f), false);
    return f;
}

void addConstant(Object *object, const QString &name, const Value &value) {
    Property &p = object->defineOwn(name);
    p.value = value;
    p.enumerable = p.writable = p.configurable = false;
}

Function *addConstructor(Runtime &runtime, const QString &name, int length, Object *prototype, Function::Native fn) {
    Function *c = runtime.newNative(name, length, std::move(fn), true);
    Property &p = c->defineOwn(QStringLiteral("prototype"));
    p.value = Value::object(prototype);
    p.enumerable = p.writable = p.configurable = false;
    prototype->setOwn(QStringLiteral("constructor"), Value::object(c), false);
    runtime.global->setOwn(name, Value::object(c), false);
    return c;
}

namespace {

Object *arrayThis(Runtime &runtime, const Value &thisValue, const char *method) {
    if (thisValue.isObject()) {
        Object *o = thisValue.asObject();
        if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments)
            return o;
    }
    runtime.throwTypeError(
        QStringLiteral("Array.prototype.%1 called on something that is not an array").arg(QLatin1String(method)));
}

qint64 relativeIndex(Runtime &runtime, const Value &v, qint64 length, qint64 fallback) {
    if (v.isUndefined())
        return fallback;
    const double d = std::trunc(runtime.toNumber(v));
    if (std::isnan(d))
        return 0;
    if (d < 0)
        return std::max<qint64>(0, length + static_cast<qint64>(std::max(d, -9e15)));
    return std::min<qint64>(length, static_cast<qint64>(std::min(d, 9e15)));
}

Function *callback(Runtime &runtime, const Value &v, const char *method) {
    Function *f = v.asFunction();
    if (!f)
        runtime.throwTypeError(QStringLiteral("%1: the callback is not a function").arg(QLatin1String(method)));
    return f;
}

QString objectClassName(const Value &v) {
    if (v.isUndefined())
        return QStringLiteral("Undefined");
    if (v.isNull())
        return QStringLiteral("Null");
    if (!v.isObject())
        return v.isString()   ? QStringLiteral("String")
               : v.isNumber() ? QStringLiteral("Number")
                              : QStringLiteral("Boolean");
    switch (v.asObject()->cls) {
    case Object::Class::Array:
        return QStringLiteral("Array");
    case Object::Class::Function:
        return QStringLiteral("Function");
    case Object::Class::Error:
        return QStringLiteral("Error");
    case Object::Class::Boolean:
        return QStringLiteral("Boolean");
    case Object::Class::Number:
        return QStringLiteral("Number");
    case Object::Class::String:
        return QStringLiteral("String");
    case Object::Class::Date:
        return QStringLiteral("Date");
    case Object::Class::RegExp:
        return QStringLiteral("RegExp");
    case Object::Class::Arguments:
        return QStringLiteral("Arguments");
    default:
        return QStringLiteral("Object");
    }
}

// Defines a property from a descriptor object, as Object.defineProperty does.
void defineFromDescriptor(Runtime &runtime, Object *o, const QString &key, const Value &descriptor) {
    if (!descriptor.isObject())
        runtime.throwTypeError(QStringLiteral("property descriptor must be an object"));
    Object *d = descriptor.asObject();
    const auto field = [&](const char *name, Value *out) {
        const QString k = QLatin1String(name);
        if (!runtime.has(d, k))
            return false;
        *out = runtime.getFromObject(d, k, descriptor);
        return true;
    };
    if (o->frozen || (!o->extensible && !o->ownProperty(key)))
        runtime.throwTypeError(QStringLiteral("cannot define property '%1': the object is not extensible").arg(key));
    Property *existing = o->ownProperty(key);
    if (existing && !existing->configurable)
        runtime.throwTypeError(QStringLiteral("cannot redefine property '%1'").arg(key));
    if ((o->cls == Object::Class::Array || o->cls == Object::Class::Arguments)) {
        quint32 index = 0;
        Value v;
        if (isArrayIndex(key, &index) || key == QLatin1String("length")) {
            if (field("value", &v))
                o->setOwn(runtime, key, v, Value::object(o));
            return;
        }
    }
    Property &p = o->defineOwn(key);
    if (!existing) {
        p.enumerable = p.writable = p.configurable = false;
    }
    Value v;
    Value getter, setter;
    const bool hasGet = field("get", &getter), hasSet = field("set", &setter);
    if (hasGet || hasSet) {
        if ((hasGet && !getter.isUndefined() && !getter.asFunction()) ||
            (hasSet && !setter.isUndefined() && !setter.asFunction()))
            runtime.throwTypeError(QStringLiteral("getters and setters must be functions"));
        p.accessor = true;
        p.value = Value();
        if (hasGet)
            p.getter = getter.asFunction();
        if (hasSet)
            p.setter = setter.asFunction();
    } else if (field("value", &v)) {
        p.accessor = false;
        p.getter = p.setter = nullptr;
        p.value = v;
    }
    if (field("enumerable", &v))
        p.enumerable = runtime.toBoolean(v);
    if (field("writable", &v))
        p.writable = runtime.toBoolean(v);
    if (field("configurable", &v))
        p.configurable = runtime.toBoolean(v);
}

void installObject(Runtime &runtime) {
    Object *proto = runtime.objectProto;
    Function *ctor = addConstructor(runtime, QStringLiteral("Object"), 1, proto,
                                    [](Runtime &rt, const Value &, const Values &args, Object *) {
                                        const Value v = argument(args, 0);
                                        if (v.isNullish())
                                            return Value::object(rt.newObject());
                                        return Value::object(rt.toObject(v));
                                    });
    addMethod(runtime, ctor, QStringLiteral("keys"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        Object *o = rt.toObject(argument(args, 0));
        Values keys;
        for (const QString &k : o->enumerableKeys(rt))
            keys.push_back(Value::string(k));
        return Value::object(rt.newArray(keys));
    });
    addMethod(runtime, ctor, QStringLiteral("values"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const Value v = argument(args, 0);
        Object *o = rt.toObject(v);
        Values values;
        for (const QString &k : o->enumerableKeys(rt))
            values.push_back(rt.getFromObject(o, k, Value::object(o)));
        return Value::object(rt.newArray(values));
    });
    addMethod(runtime, ctor, QStringLiteral("entries"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  Object *o = rt.toObject(argument(args, 0));
                  Values entries;
                  for (const QString &k : o->enumerableKeys(rt))
                      entries.push_back(
                          Value::object(rt.newArray({Value::string(k), rt.getFromObject(o, k, Value::object(o))})));
                  return Value::object(rt.newArray(entries));
              });
    addMethod(runtime, ctor, QStringLiteral("fromEntries"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  Object *o = rt.newObject();
                  const Value list = argument(args, 0);
                  if (!list.isObject())
                      rt.throwTypeError(QStringLiteral("Object.fromEntries needs an array"));
                  for (const Value &entry : list.asObject()->elements) {
                      const Value k = rt.get(entry, QStringLiteral("0")), v = rt.get(entry, QStringLiteral("1"));
                      o->setOwn(rt.toString(k), v);
                  }
                  return Value::object(o);
              });
    addMethod(runtime, ctor, QStringLiteral("assign"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const Value target = Value::object(rt.toObject(argument(args, 0)));
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i].isNullish())
                continue;
            Object *s = rt.toObject(args[i]);
            for (const QString &k : s->enumerableKeys(rt))
                rt.put(target, k, rt.getFromObject(s, k, args[i]));
        }
        return target;
    });
    addMethod(runtime, ctor, QStringLiteral("create"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const Value p = argument(args, 0);
        if (!p.isObject() && !p.isNull())
            rt.throwTypeError(QStringLiteral("the prototype must be an object or null"));
        Object *o = rt.heap.make<Object>(p.isNull() ? nullptr : p.asObject());
        const Value props = argument(args, 1);
        if (props.isObject())
            for (const QString &k : props.asObject()->enumerableKeys(rt))
                defineFromDescriptor(rt, o, k, rt.get(props, k));
        return Value::object(o);
    });
    addMethod(runtime, ctor, QStringLiteral("getPrototypeOf"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  Object *o = rt.toObject(argument(args, 0));
                  return o->proto ? Value::object(o->proto) : Value::null();
              });
    addMethod(runtime, ctor, QStringLiteral("setPrototypeOf"), 2,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  const Value o = argument(args, 0), p = argument(args, 1);
                  if (!p.isObject() && !p.isNull())
                      rt.throwTypeError(QStringLiteral("the prototype must be an object or null"));
                  if (o.isObject()) {
                      for (Object *c = p.isNull() ? nullptr : p.asObject(); c; c = c->proto)
                          if (c == o.asObject())
                              rt.throwTypeError(QStringLiteral("cyclic prototype chain"));
                      o.asObject()->proto = p.isNull() ? nullptr : p.asObject();
                  }
                  return o;
              });
    addMethod(runtime, ctor, QStringLiteral("defineProperty"), 3,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  const Value o = argument(args, 0);
                  if (!o.isObject())
                      rt.throwTypeError(QStringLiteral("Object.defineProperty called on a non-object"));
                  defineFromDescriptor(rt, o.asObject(), rt.toString(argument(args, 1)), argument(args, 2));
                  return o;
              });
    addMethod(runtime, ctor, QStringLiteral("defineProperties"), 2,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  const Value o = argument(args, 0), props = argument(args, 1);
                  if (!o.isObject())
                      rt.throwTypeError(QStringLiteral("Object.defineProperties called on a non-object"));
                  Object *p = rt.toObject(props);
                  for (const QString &k : p->enumerableKeys(rt))
                      defineFromDescriptor(rt, o.asObject(), k, rt.getFromObject(p, k, props));
                  return o;
              });
    addMethod(runtime, ctor, QStringLiteral("getOwnPropertyNames"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  Object *o = rt.toObject(argument(args, 0));
                  Values keys;
                  if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments) {
                      for (std::size_t i = 0; i < o->elements.size(); ++i)
                          keys.push_back(Value::string(QString::number(i)));
                      keys.push_back(Value::string(QStringLiteral("length")));
                  }
                  for (const QString &k : o->ownPropertyKeys())
                      keys.push_back(Value::string(k));
                  return Value::object(rt.newArray(keys));
              });
    addMethod(runtime, ctor, QStringLiteral("getOwnPropertyDescriptor"), 2,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  Object *o = rt.toObject(argument(args, 0));
                  const QString key = rt.toString(argument(args, 1));
                  const Property *p = o->ownProperty(key);
                  if (!p) {
                      Value v;
                      if (!o->getOwn(rt, key, Value::object(o), &v))
                          return Value();
                      Object *d = rt.newObject();
                      d->setOwn(QStringLiteral("value"), v);
                      d->setOwn(QStringLiteral("writable"), Value::boolean(!o->frozen));
                      d->setOwn(QStringLiteral("enumerable"), Value::boolean(key != QLatin1String("length")));
                      d->setOwn(QStringLiteral("configurable"), Value::boolean(false));
                      return Value::object(d);
                  }
                  Object *d = rt.newObject();
                  if (p->accessor) {
                      d->setOwn(QStringLiteral("get"), p->getter ? Value::object(p->getter) : Value());
                      d->setOwn(QStringLiteral("set"), p->setter ? Value::object(p->setter) : Value());
                  } else {
                      d->setOwn(QStringLiteral("value"), p->value);
                      d->setOwn(QStringLiteral("writable"), Value::boolean(p->writable && !o->frozen));
                  }
                  d->setOwn(QStringLiteral("enumerable"), Value::boolean(p->enumerable));
                  d->setOwn(QStringLiteral("configurable"), Value::boolean(p->configurable && !o->frozen));
                  return Value::object(d);
              });
    addMethod(runtime, ctor, QStringLiteral("freeze"), 1, [](Runtime &, const Value &, const Values &args, Object *) {
        const Value o = argument(args, 0);
        if (o.isObject()) {
            o.asObject()->frozen = true;
            o.asObject()->extensible = false;
        }
        return o;
    });
    addMethod(runtime, ctor, QStringLiteral("isFrozen"), 1, [](Runtime &, const Value &, const Values &args, Object *) {
        const Value o = argument(args, 0);
        return Value::boolean(!o.isObject() || o.asObject()->frozen);
    });
    const auto preventExtensions = [](Runtime &, const Value &, const Values &args, Object *) {
        const Value o = argument(args, 0);
        if (o.isObject())
            o.asObject()->extensible = false;
        return o;
    };
    addMethod(runtime, ctor, QStringLiteral("seal"), 1, preventExtensions);
    addMethod(runtime, ctor, QStringLiteral("preventExtensions"), 1, preventExtensions);
    addMethod(runtime, ctor, QStringLiteral("isExtensible"), 1,
              [](Runtime &, const Value &, const Values &args, Object *) {
                  const Value o = argument(args, 0);
                  return Value::boolean(o.isObject() && o.asObject()->extensible);
              });
    addMethod(runtime, ctor, QStringLiteral("is"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const Value a = argument(args, 0), b = argument(args, 1);
        if (a.isNumber() && b.isNumber()) {
            const double x = a.asNumber(), y = b.asNumber();
            if (std::isnan(x) && std::isnan(y))
                return Value::boolean(true);
            if (x == 0 && y == 0)
                return Value::boolean(std::signbit(x) == std::signbit(y));
        }
        return Value::boolean(rt.strictEquals(a, b));
    });

    addMethod(runtime, proto, QStringLiteral("hasOwnProperty"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString key = rt.toString(argument(args, 0));
                  return Value::boolean(rt.toObject(self)->hasOwn(rt, key));
              });
    addMethod(runtime, proto, QStringLiteral("isPrototypeOf"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const Value v = argument(args, 0);
                  if (!v.isObject())
                      return Value::boolean(false);
                  Object *me = rt.toObject(self);
                  for (Object *o = v.asObject()->proto; o; o = o->proto)
                      if (o == me)
                          return Value::boolean(true);
                  return Value::boolean(false);
              });
    addMethod(runtime, proto, QStringLiteral("propertyIsEnumerable"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *o = rt.toObject(self);
                  return Value::boolean(o->enumerableKeys(rt).contains(rt.toString(argument(args, 0))));
              });
    const auto toStringFn = [](Runtime &, const Value &self, const Values &, Object *) {
        return Value::string(QStringLiteral("[object %1]").arg(objectClassName(self)));
    };
    addMethod(runtime, proto, QStringLiteral("toString"), 0, toStringFn);
    addMethod(runtime, proto, QStringLiteral("toLocaleString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  return rt.call(rt.get(self, QStringLiteral("toString")), self, {});
              });
    addMethod(
        runtime, proto, QStringLiteral("valueOf"), 0,
        [](Runtime &rt, const Value &self, const Values &, Object *) { return Value::object(rt.toObject(self)); });
}

void installFunction(Runtime &runtime) {
    Function *proto = runtime.functionProto;
    addConstructor(
        runtime, QStringLiteral("Function"), 1, proto, [](Runtime &rt, const Value &, const Values &, Object *) {
            rt.throwError(ErrorType::EvalError, QStringLiteral("making functions from strings is not supported"));
            return Value();
        });
    proto->setOwn(QStringLiteral("name"), Value::string(QString()), false);
    addMethod(runtime, proto, QStringLiteral("call"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const Values rest(args.size() > 1 ? args.begin() + 1 : args.end(), args.end());
                  return rt.call(self, argument(args, 0), rest);
              });
    addMethod(runtime, proto, QStringLiteral("apply"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const Value list = argument(args, 1);
                  Values call;
                  if (list.isObject()) {
                      Object *o = list.asObject();
                      if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments) {
                          call = o->elements;
                      } else {
                          const double n = rt.toNumber(rt.getFromObject(o, QStringLiteral("length"), list));
                          for (int i = 0; i < static_cast<int>(std::min(n, 1e6)); ++i)
                              call.push_back(rt.getFromObject(o, QString::number(i), list));
                      }
                  } else if (!list.isNullish()) {
                      rt.throwTypeError(QStringLiteral("the arguments of apply() must be an array"));
                  }
                  return rt.call(self, argument(args, 0), call);
              });
    addMethod(runtime, proto, QStringLiteral("bind"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Function *target = self.asFunction();
                  if (!target)
                      rt.throwTypeError(QStringLiteral("bind() called on something that is not a function"));
                  Function *f = rt.heap.make<Function>(rt.functionProto);
                  f->target = target;
                  f->boundThis = argument(args, 0);
                  f->boundArgs.assign(args.size() > 1 ? args.begin() + 1 : args.end(), args.end());
                  f->constructor = target->constructor;
                  f->name = QStringLiteral("bound ") + target->name;
                  f->setOwn(QStringLiteral("name"), Value::string(f->name), false);
                  return Value::object(f);
              });
    addMethod(runtime, proto, QStringLiteral("toString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  Function *f = self.asFunction();
                  if (!f)
                      rt.throwTypeError(
                          QStringLiteral("Function.prototype.toString called on something that is not a function"));
                  if (f->node && f->program && f->node->sourceEnd > f->node->sourceStart)
                      return Value::string(
                          f->program->source.mid(f->node->sourceStart, f->node->sourceEnd - f->node->sourceStart));
                  return Value::string(QStringLiteral("function %1() { [native code] }").arg(f->name));
              });
}

int compareForSort(Runtime &runtime, const Value &compare, const Value &a, const Value &b) {
    // undefined goes last.
    if (a.isUndefined())
        return b.isUndefined() ? 0 : 1;
    if (b.isUndefined())
        return -1;
    if (!compare.isUndefined()) {
        const double r = runtime.toNumber(runtime.call(compare, Value(), {a, b}));
        return r < 0 ? -1 : r > 0 ? 1 : 0;
    }
    const QString x = runtime.toString(a), y = runtime.toString(b);
    return x < y ? -1 : y < x ? 1 : 0;
}

void flatten(Runtime &runtime, const Values &in, double depth, Values *out) {
    for (const Value &v : in) {
        if (depth >= 1 && v.isObject() && v.asObject()->cls == Object::Class::Array)
            flatten(runtime, v.asObject()->elements, depth - 1, out);
        else
            out->push_back(v);
    }
}

void installArray(Runtime &runtime) {
    Object *proto = runtime.heap.make<Object>(runtime.objectProto, Object::Class::Array);
    runtime.arrayProto = proto;
    Function *ctor = addConstructor(
        runtime, QStringLiteral("Array"), 1, proto, [](Runtime &rt, const Value &, const Values &args, Object *) {
            if (args.size() == 1 && args[0].isNumber()) {
                const double n = args[0].asNumber();
                if (n < 0 || n != std::floor(n) || n > 4294967295.0)
                    rt.throwError(ErrorType::RangeError, QStringLiteral("invalid array length"));
                if (n > 1e7)
                    rt.throwError(ErrorType::RangeError, QStringLiteral("array length too large"));
                return Value::object(rt.newArray(Values(static_cast<std::size_t>(n))));
            }
            return Value::object(rt.newArray(args));
        });
    addMethod(runtime, ctor, QStringLiteral("isArray"), 1, [](Runtime &, const Value &, const Values &args, Object *) {
        const Value v = argument(args, 0);
        return Value::boolean(v.isObject() && v.asObject()->cls == Object::Class::Array);
    });
    addMethod(runtime, ctor, QStringLiteral("of"), 0, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::object(rt.newArray(args));
    });
    addMethod(runtime, ctor, QStringLiteral("from"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const Value source = argument(args, 0), map = argument(args, 1);
        Values items;
        if (source.isString()) {
            for (const QChar c : source.asString())
                items.push_back(Value::string(QString(c)));
        } else if (source.isObject()) {
            Object *o = source.asObject();
            if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments) {
                items = o->elements;
            } else {
                const double n = rt.toNumber(rt.getFromObject(o, QStringLiteral("length"), source));
                for (int i = 0; i < static_cast<int>(std::isnan(n) ? 0 : std::min(n, 1e7)); ++i)
                    items.push_back(rt.getFromObject(o, QString::number(i), source));
            }
        }
        if (!map.isUndefined()) {
            callback(rt, map, "from");
            for (std::size_t i = 0; i < items.size(); ++i)
                items[i] = rt.call(map, argument(args, 2), {items[i], Value::number(static_cast<double>(i))});
        }
        return Value::object(rt.newArray(items));
    });

    addMethod(runtime, proto, QStringLiteral("push"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "push");
                  if (a->frozen)
                      rt.throwTypeError(QStringLiteral("cannot add to a frozen array"));
                  a->elements.insert(a->elements.end(), args.begin(), args.end());
                  return Value::number(static_cast<double>(a->elements.size()));
              });
    addMethod(runtime, proto, QStringLiteral("pop"), 0, [](Runtime &rt, const Value &self, const Values &, Object *) {
        Object *a = arrayThis(rt, self, "pop");
        if (a->elements.empty() || a->frozen)
            return Value();
        const Value v = a->elements.back();
        a->elements.pop_back();
        return v;
    });
    addMethod(runtime, proto, QStringLiteral("shift"), 0, [](Runtime &rt, const Value &self, const Values &, Object *) {
        Object *a = arrayThis(rt, self, "shift");
        if (a->elements.empty() || a->frozen)
            return Value();
        const Value v = a->elements.front();
        a->elements.erase(a->elements.begin());
        return v;
    });
    addMethod(runtime, proto, QStringLiteral("unshift"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "unshift");
                  if (!a->frozen)
                      a->elements.insert(a->elements.begin(), args.begin(), args.end());
                  return Value::number(static_cast<double>(a->elements.size()));
              });
    addMethod(runtime, proto, QStringLiteral("slice"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "slice");
                  const auto n = static_cast<qint64>(a->elements.size());
                  const qint64 b = relativeIndex(rt, argument(args, 0), n, 0);
                  const qint64 e = relativeIndex(rt, argument(args, 1), n, n);
                  Values out;
                  for (qint64 i = b; i < e; ++i)
                      out.push_back(a->elements[static_cast<std::size_t>(i)]);
                  return Value::object(rt.newArray(out));
              });
    addMethod(runtime, proto, QStringLiteral("splice"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "splice");
                  if (a->frozen)
                      rt.throwTypeError(QStringLiteral("cannot change a frozen array"));
                  const auto n = static_cast<qint64>(a->elements.size());
                  const qint64 start = relativeIndex(rt, argument(args, 0), n, 0);
                  qint64 count = n - start;
                  if (args.size() >= 2) {
                      const double c = std::trunc(rt.toNumber(args[1]));
                      count = std::isnan(c)
                                  ? 0
                                  : std::clamp<qint64>(static_cast<qint64>(std::clamp(c, -1e15, 1e15)), 0, n - start);
                  } else if (args.empty()) {
                      count = 0;
                  }
                  const auto first = a->elements.begin() + start;
                  Values removed(first, first + count);
                  a->elements.erase(first, first + count);
                  if (args.size() > 2)
                      a->elements.insert(a->elements.begin() + start, args.begin() + 2, args.end());
                  return Value::object(rt.newArray(removed));
              });
    addMethod(runtime, proto, QStringLiteral("concat"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "concat");
                  Values out = a->elements;
                  for (const Value &v : args) {
                      if (v.isObject() && v.asObject()->cls == Object::Class::Array)
                          out.insert(out.end(), v.asObject()->elements.begin(), v.asObject()->elements.end());
                      else
                          out.push_back(v);
                  }
                  return Value::object(rt.newArray(out));
              });
    const auto join = [](Runtime &rt, const Value &self, const Values &args, Object *) {
        Object *a = arrayThis(rt, self, "join");
        const Value sep = argument(args, 0);
        const QString separator = sep.isUndefined() ? QStringLiteral(",") : rt.toString(sep);
        // Arrays that contain themselves are joined as empty strings, not forever.
        static thread_local QList<Object *> active;
        if (active.contains(a) || active.size() > 100)
            return Value::string(QString());
        active.append(a);
        QString out;
        try {
            for (std::size_t i = 0; i < a->elements.size(); ++i) {
                if (i)
                    out += separator;
                const Value &v = a->elements[i];
                if (!v.isNullish())
                    out += rt.toString(v);
            }
        } catch (...) {
            active.removeAll(a);
            throw;
        }
        active.removeAll(a);
        return Value::string(out);
    };
    addMethod(runtime, proto, QStringLiteral("join"), 1, join);
    addMethod(runtime, proto, QStringLiteral("toString"), 0, join);
    addMethod(runtime, proto, QStringLiteral("toLocaleString"), 0, join);
    addMethod(runtime, proto, QStringLiteral("reverse"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  Object *a = arrayThis(rt, self, "reverse");
                  std::reverse(a->elements.begin(), a->elements.end());
                  return self;
              });
    const auto indexOf = [](bool last) {
        return [last](Runtime &rt, const Value &self, const Values &args, Object *) {
            Object *a = arrayThis(rt, self, last ? "lastIndexOf" : "indexOf");
            const auto n = static_cast<qint64>(a->elements.size());
            const Value target = argument(args, 0);
            if (last) {
                qint64 from = n - 1;
                if (args.size() > 1) {
                    const double f = std::trunc(rt.toNumber(args[1]));
                    from = f < 0 ? n + static_cast<qint64>(std::max(f, -9e15)) : static_cast<qint64>(std::min(f, 9e15));
                    from = std::min(from, n - 1);
                }
                for (qint64 i = from; i >= 0; --i)
                    if (rt.strictEquals(a->elements[static_cast<std::size_t>(i)], target))
                        return Value::number(static_cast<double>(i));
            } else {
                for (qint64 i = relativeIndex(rt, argument(args, 1), n, 0); i < n; ++i)
                    if (rt.strictEquals(a->elements[static_cast<std::size_t>(i)], target))
                        return Value::number(static_cast<double>(i));
            }
            return Value::number(-1);
        };
    };
    addMethod(runtime, proto, QStringLiteral("indexOf"), 1, indexOf(false));
    addMethod(runtime, proto, QStringLiteral("lastIndexOf"), 1, indexOf(true));
    addMethod(runtime, proto, QStringLiteral("includes"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "includes");
                  const auto n = static_cast<qint64>(a->elements.size());
                  for (qint64 i = relativeIndex(rt, argument(args, 1), n, 0); i < n; ++i)
                      if (rt.sameValueZero(a->elements[static_cast<std::size_t>(i)], argument(args, 0)))
                          return Value::boolean(true);
                  return Value::boolean(false);
              });
    addMethod(runtime, proto, QStringLiteral("at"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "at");
                  const double d = std::trunc(rt.toNumber(argument(args, 0)));
                  const auto n = static_cast<double>(a->elements.size());
                  const double i = std::isnan(d) ? 0 : d < 0 ? n + d : d;
                  if (i < 0 || i >= n)
                      return Value();
                  return a->elements[static_cast<std::size_t>(i)];
              });

    // The iterating methods read the length once and skip elements removed while they run.
    enum class Kind { ForEach, Map, Filter, Some, Every, Find, FindIndex, FindLast, FindLastIndex, FlatMap };
    const auto iterate = [](Kind kind, const char *name) {
        return [kind, name](Runtime &rt, const Value &self, const Values &args, Object *) {
            Object *a = arrayThis(rt, self, name);
            const Value fn = argument(args, 0), thisArg = argument(args, 1);
            callback(rt, fn, name);
            const std::size_t n = a->elements.size();
            Values out;
            const bool backwards = kind == Kind::FindLast || kind == Kind::FindLastIndex;
            for (std::size_t k = 0; k < n; ++k) {
                const std::size_t i = backwards ? n - 1 - k : k;
                if (i >= a->elements.size())
                    continue;
                Value v = a->elements[i];
                const Value r = rt.call(fn, thisArg, {v, Value::number(static_cast<double>(i)), self});
                switch (kind) {
                case Kind::ForEach:
                    break;
                case Kind::Map:
                    out.push_back(r);
                    break;
                case Kind::FlatMap:
                    if (r.isObject() && r.asObject()->cls == Object::Class::Array)
                        out.insert(out.end(), r.asObject()->elements.begin(), r.asObject()->elements.end());
                    else
                        out.push_back(r);
                    break;
                case Kind::Filter:
                    if (rt.toBoolean(r))
                        out.push_back(v);
                    break;
                case Kind::Some:
                    if (rt.toBoolean(r))
                        return Value::boolean(true);
                    break;
                case Kind::Every:
                    if (!rt.toBoolean(r))
                        return Value::boolean(false);
                    break;
                case Kind::Find:
                case Kind::FindLast:
                    if (rt.toBoolean(r))
                        return v;
                    break;
                case Kind::FindIndex:
                case Kind::FindLastIndex:
                    if (rt.toBoolean(r))
                        return Value::number(static_cast<double>(i));
                    break;
                }
            }
            switch (kind) {
            case Kind::Map:
            case Kind::Filter:
            case Kind::FlatMap:
                return Value::object(rt.newArray(out));
            case Kind::Some:
                return Value::boolean(false);
            case Kind::Every:
                return Value::boolean(true);
            case Kind::FindIndex:
            case Kind::FindLastIndex:
                return Value::number(-1);
            default:
                return Value();
            }
        };
    };
    addMethod(runtime, proto, QStringLiteral("forEach"), 1, iterate(Kind::ForEach, "forEach"));
    addMethod(runtime, proto, QStringLiteral("map"), 1, iterate(Kind::Map, "map"));
    addMethod(runtime, proto, QStringLiteral("filter"), 1, iterate(Kind::Filter, "filter"));
    addMethod(runtime, proto, QStringLiteral("some"), 1, iterate(Kind::Some, "some"));
    addMethod(runtime, proto, QStringLiteral("every"), 1, iterate(Kind::Every, "every"));
    addMethod(runtime, proto, QStringLiteral("find"), 1, iterate(Kind::Find, "find"));
    addMethod(runtime, proto, QStringLiteral("findIndex"), 1, iterate(Kind::FindIndex, "findIndex"));
    addMethod(runtime, proto, QStringLiteral("findLast"), 1, iterate(Kind::FindLast, "findLast"));
    addMethod(runtime, proto, QStringLiteral("findLastIndex"), 1, iterate(Kind::FindLastIndex, "findLastIndex"));
    addMethod(runtime, proto, QStringLiteral("flatMap"), 1, iterate(Kind::FlatMap, "flatMap"));
    const auto reduce = [](bool right) {
        return [right](Runtime &rt, const Value &self, const Values &args, Object *) {
            Object *a = arrayThis(rt, self, right ? "reduceRight" : "reduce");
            const Value fn = argument(args, 0);
            callback(rt, fn, right ? "reduceRight" : "reduce");
            const std::size_t n = a->elements.size();
            std::size_t k = 0;
            Value acc;
            if (args.size() >= 2) {
                acc = args[1];
            } else {
                if (n == 0)
                    rt.throwTypeError(QStringLiteral("reduce of an empty array with no initial value"));
                acc = a->elements[right ? n - 1 : 0];
                k = 1;
            }
            for (; k < n; ++k) {
                const std::size_t i = right ? n - 1 - k : k;
                if (i >= a->elements.size())
                    continue;
                acc = rt.call(fn, Value(), {acc, a->elements[i], Value::number(static_cast<double>(i)), self});
            }
            return acc;
        };
    };
    addMethod(runtime, proto, QStringLiteral("reduce"), 1, reduce(false));
    addMethod(runtime, proto, QStringLiteral("reduceRight"), 1, reduce(true));
    addMethod(
        runtime, proto, QStringLiteral("sort"), 1, [](Runtime &rt, const Value &self, const Values &args, Object *) {
            Object *a = arrayThis(rt, self, "sort");
            const Value compare = argument(args, 0);
            if (!compare.isUndefined())
                callback(rt, compare, "sort");
            // A merge sort on a copy: the comparison may throw or change the array.
            Values items = a->elements;
            Values buffer(items.size());
            for (std::size_t width = 1; width < items.size(); width *= 2) {
                for (std::size_t lo = 0; lo < items.size(); lo += 2 * width) {
                    const std::size_t mid = std::min(lo + width, items.size());
                    const std::size_t hi = std::min(lo + 2 * width, items.size());
                    std::size_t i = lo, j = mid, k = lo;
                    while (i < mid && j < hi)
                        buffer[k++] = compareForSort(rt, compare, items[j], items[i]) < 0 ? items[j++] : items[i++];
                    while (i < mid)
                        buffer[k++] = items[i++];
                    while (j < hi)
                        buffer[k++] = items[j++];
                }
                items.swap(buffer);
            }
            a->elements = items;
            return self;
        });
    addMethod(runtime, proto, QStringLiteral("fill"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "fill");
                  const auto n = static_cast<qint64>(a->elements.size());
                  const qint64 b = relativeIndex(rt, argument(args, 1), n, 0);
                  const qint64 e = relativeIndex(rt, argument(args, 2), n, n);
                  for (qint64 i = b; i < e; ++i)
                      a->elements[static_cast<std::size_t>(i)] = argument(args, 0);
                  return self;
              });
    addMethod(runtime, proto, QStringLiteral("flat"), 0,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  Object *a = arrayThis(rt, self, "flat");
                  const Value d = argument(args, 0);
                  const double depth = d.isUndefined() ? 1.0 : std::min(rt.toNumber(d), 100.0);
                  Values out;
                  flatten(rt, a->elements, depth, &out);
                  return Value::object(rt.newArray(out));
              });
    addMethod(runtime, proto, QStringLiteral("keys"), 0, [](Runtime &rt, const Value &self, const Values &, Object *) {
        Object *a = arrayThis(rt, self, "keys");
        Values out;
        for (std::size_t i = 0; i < a->elements.size(); ++i)
            out.push_back(Value::number(static_cast<double>(i)));
        return Value::object(rt.newArray(out));
    });
    addMethod(runtime, proto, QStringLiteral("values"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  return Value::object(rt.newArray(arrayThis(rt, self, "values")->elements));
              });
    addMethod(
        runtime, proto, QStringLiteral("entries"), 0, [](Runtime &rt, const Value &self, const Values &, Object *) {
            Object *a = arrayThis(rt, self, "entries");
            Values out;
            for (std::size_t i = 0; i < a->elements.size(); ++i)
                out.push_back(Value::object(rt.newArray({Value::number(static_cast<double>(i)), a->elements[i]})));
            return Value::object(rt.newArray(out));
        });
}

void installBoolean(Runtime &runtime) {
    Object *proto = runtime.heap.make<Object>(runtime.objectProto, Object::Class::Boolean);
    proto->primitive = Value::boolean(false);
    runtime.booleanProto = proto;
    addConstructor(runtime, QStringLiteral("Boolean"), 1, proto,
                   [](Runtime &rt, const Value &, const Values &args, Object *newTarget) {
                       Value b = Value::boolean(rt.toBoolean(argument(args, 0)));
                       if (!newTarget)
                           return b;
                       return Value::object(rt.toObject(b));
                   });
    const auto unwrap = [](Runtime &rt, const Value &self) {
        if (self.isBoolean())
            return self.asBoolean();
        if (self.isObject() && self.asObject()->cls == Object::Class::Boolean)
            return self.asObject()->primitive.asBoolean();
        rt.throwTypeError(QStringLiteral("not a boolean"));
    };
    addMethod(runtime, proto, QStringLiteral("toString"), 0,
              [unwrap](Runtime &rt, const Value &self, const Values &, Object *) {
                  return Value::string(unwrap(rt, self) ? QStringLiteral("true") : QStringLiteral("false"));
              });
    addMethod(runtime, proto, QStringLiteral("valueOf"), 0,
              [unwrap](Runtime &rt, const Value &self, const Values &, Object *) {
                  return Value::boolean(unwrap(rt, self));
              });
}

void installErrors(Runtime &runtime) {
    static const char *const names[] = {"Error",       "EvalError", "RangeError", "ReferenceError",
                                        "SyntaxError", "TypeError", "URIError"};
    for (int i = 0; i < 7; ++i) {
        Object *proto = runtime.heap.make<Object>(i == 0 ? runtime.objectProto : runtime.errorProto);
        runtime.errorProtos[i] = proto;
        if (i == 0)
            runtime.errorProto = proto;
        proto->setOwn(QStringLiteral("name"), Value::string(QLatin1String(names[i])), false);
        proto->setOwn(QStringLiteral("message"), Value::string(QString()), false);
        const auto type = static_cast<ErrorType>(i);
        Function *ctor =
            addConstructor(runtime, QLatin1String(names[i]), 1, proto,
                           [type](Runtime &rt, const Value &, const Values &args, Object *newTarget) {
                               const Value m = argument(args, 0);
                               Object *e = rt.newError(type, m.isUndefined() ? QString() : rt.toString(m));
                               if (m.isUndefined())
                                   e->removeOwn(QStringLiteral("message"));
                               if (newTarget) {
                                   const Value p = rt.get(Value::object(newTarget), QStringLiteral("prototype"));
                                   if (p.isObject())
                                       e->proto = p.asObject();
                               }
                               const Value options = argument(args, 1);
                               if (options.isObject() && rt.has(options.asObject(), QStringLiteral("cause")))
                                   e->setOwn(QStringLiteral("cause"), rt.get(options, QStringLiteral("cause")), false);
                               return Value::object(e);
                           });
        if (i > 0)
            ctor->proto = runtime.global->ownProperty(QStringLiteral("Error"))->value.asObject();
    }
    addMethod(runtime, runtime.errorProto, QStringLiteral("toString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  if (!self.isObject())
                      rt.throwTypeError(QStringLiteral("Error.prototype.toString called on a non-object"));
                  const Value n = rt.get(self, QStringLiteral("name")), m = rt.get(self, QStringLiteral("message"));
                  const QString name = n.isUndefined() ? QStringLiteral("Error") : rt.toString(n);
                  const QString message = m.isUndefined() ? QString() : rt.toString(m);
                  if (name.isEmpty())
                      return Value::string(message);
                  if (message.isEmpty())
                      return Value::string(name);
                  return Value::string(name + QStringLiteral(": ") + message);
              });
}

void installMath(Runtime &runtime) {
    Object *math = runtime.newObject();
    runtime.global->setOwn(QStringLiteral("Math"), Value::object(math), false);
    addConstant(math, QStringLiteral("E"), Value::number(2.718281828459045));
    addConstant(math, QStringLiteral("LN10"), Value::number(2.302585092994046));
    addConstant(math, QStringLiteral("LN2"), Value::number(0.6931471805599453));
    addConstant(math, QStringLiteral("LOG10E"), Value::number(0.4342944819032518));
    addConstant(math, QStringLiteral("LOG2E"), Value::number(1.4426950408889634));
    addConstant(math, QStringLiteral("PI"), Value::number(3.141592653589793));
    addConstant(math, QStringLiteral("SQRT1_2"), Value::number(0.7071067811865476));
    addConstant(math, QStringLiteral("SQRT2"), Value::number(1.4142135623730951));
    const auto unary = [&](const char *name, double (*fn)(double)) {
        addMethod(runtime, math, QLatin1String(name), 1,
                  [fn](Runtime &rt, const Value &, const Values &args, Object *) {
                      return Value::number(fn(rt.toNumber(argument(args, 0))));
                  });
    };
    unary("abs", [](double x) { return std::fabs(x); });
    unary("acos", [](double x) { return std::acos(x); });
    unary("acosh", [](double x) { return std::acosh(x); });
    unary("asin", [](double x) { return std::asin(x); });
    unary("asinh", [](double x) { return std::asinh(x); });
    unary("atan", [](double x) { return std::atan(x); });
    unary("atanh", [](double x) { return std::atanh(x); });
    unary("cbrt", [](double x) { return std::cbrt(x); });
    unary("ceil", [](double x) { return std::ceil(x); });
    unary("cos", [](double x) { return std::cos(x); });
    unary("cosh", [](double x) { return std::cosh(x); });
    unary("exp", [](double x) { return std::exp(x); });
    unary("expm1", [](double x) { return std::expm1(x); });
    unary("floor", [](double x) { return std::floor(x); });
    unary("fround", [](double x) { return static_cast<double>(static_cast<float>(x)); });
    unary("log", [](double x) { return std::log(x); });
    unary("log1p", [](double x) { return std::log1p(x); });
    unary("log10", [](double x) { return std::log10(x); });
    unary("log2", [](double x) { return std::log2(x); });
    unary("round", [](double x) {
        if (std::isnan(x) || std::isinf(x))
            return x;
        const double r = std::floor(x + 0.5);
        return r == 0 && std::signbit(x) ? -0.0 : r;
    });
    unary("sign", [](double x) { return std::isnan(x) ? x : x > 0 ? 1.0 : x < 0 ? -1.0 : x; });
    unary("sin", [](double x) { return std::sin(x); });
    unary("sinh", [](double x) { return std::sinh(x); });
    unary("sqrt", [](double x) { return std::sqrt(x); });
    unary("tan", [](double x) { return std::tan(x); });
    unary("tanh", [](double x) { return std::tanh(x); });
    unary("trunc", [](double x) { return std::trunc(x); });
    unary("clz32", [](double x) {
        quint32 v = static_cast<quint32>(
            std::isfinite(x) ? std::fmod(std::trunc(x), 4294967296.0) + (x < 0 ? 4294967296.0 : 0) : 0);
        int n = 0;
        for (int i = 31; i >= 0 && !(v & (1u << i)); --i)
            ++n;
        return static_cast<double>(n);
    });
    addMethod(runtime, math, QStringLiteral("atan2"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::number(std::atan2(rt.toNumber(argument(args, 0)), rt.toNumber(argument(args, 1))));
    });
    addMethod(runtime, math, QStringLiteral("pow"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const double x = rt.toNumber(argument(args, 0)), y = rt.toNumber(argument(args, 1));
        if (std::isnan(y) || (std::fabs(x) == 1.0 && std::isinf(y)))
            return Value::number(std::numeric_limits<double>::quiet_NaN());
        return Value::number(std::pow(x, y));
    });
    addMethod(runtime, math, QStringLiteral("imul"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const auto toU = [&](const Value &v) {
            const double d = rt.toNumber(v);
            if (!std::isfinite(d))
                return 0u;
            double m = std::fmod(std::trunc(d), 4294967296.0);
            if (m < 0)
                m += 4294967296.0;
            return static_cast<quint32>(m);
        };
        return Value::number(static_cast<qint32>(toU(argument(args, 0)) * toU(argument(args, 1))));
    });
    addMethod(runtime, math, QStringLiteral("hypot"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        double sum = 0.0;
        bool inf = false;
        for (const Value &v : args) {
            const double d = rt.toNumber(v);
            inf = inf || std::isinf(d);
            sum += d * d;
        }
        return Value::number(inf ? std::numeric_limits<double>::infinity() : std::sqrt(sum));
    });
    addMethod(runtime, math, QStringLiteral("max"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        double m = -std::numeric_limits<double>::infinity();
        bool nan = false;
        for (const Value &v : args) {
            const double d = rt.toNumber(v);
            nan = nan || std::isnan(d);
            if (d > m || (d == 0 && m == 0 && !std::signbit(d)))
                m = d;
        }
        return Value::number(nan ? std::numeric_limits<double>::quiet_NaN() : m);
    });
    addMethod(runtime, math, QStringLiteral("min"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        double m = std::numeric_limits<double>::infinity();
        bool nan = false;
        for (const Value &v : args) {
            const double d = rt.toNumber(v);
            nan = nan || std::isnan(d);
            if (d < m || (d == 0 && m == 0 && std::signbit(d)))
                m = d;
        }
        return Value::number(nan ? std::numeric_limits<double>::quiet_NaN() : m);
    });
    addMethod(runtime, math, QStringLiteral("random"), 0, [](Runtime &, const Value &, const Values &, Object *) {
        return Value::number(QRandomGenerator::global()->generateDouble());
    });
}

QString uriEncode(Runtime &runtime, const QString &s, const char *unreserved) {
    QString out;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c.unicode() < 128 && (c.isLetterOrNumber() || std::strchr(unreserved, static_cast<char>(c.unicode())))) {
            out += c;
            continue;
        }
        QString one(c);
        if (c.isHighSurrogate()) {
            if (i + 1 >= s.size() || !s.at(i + 1).isLowSurrogate())
                runtime.throwError(ErrorType::URIError, QStringLiteral("malformed URI"));
            one += s.at(++i);
        } else if (c.isLowSurrogate()) {
            runtime.throwError(ErrorType::URIError, QStringLiteral("malformed URI"));
        }
        for (const char b : one.toUtf8())
            out += u'%' + QStringLiteral("%1").arg(static_cast<uchar>(b), 2, 16, QLatin1Char('0')).toUpper();
    }
    return out;
}

QString uriDecode(Runtime &runtime, const QString &s, const char *reserved) {
    QString out;
    for (qsizetype i = 0; i < s.size();) {
        if (s.at(i) != u'%') {
            out += s.at(i++);
            continue;
        }
        QByteArray bytes;
        const qsizetype start = i;
        while (i < s.size() && s.at(i) == u'%') {
            if (i + 2 >= s.size())
                runtime.throwError(ErrorType::URIError, QStringLiteral("malformed URI"));
            bool ok = false;
            const int b = s.mid(i + 1, 2).toInt(&ok, 16);
            if (!ok)
                runtime.throwError(ErrorType::URIError, QStringLiteral("malformed URI"));
            bytes.append(static_cast<char>(b));
            i += 3;
            // A single ASCII byte ends the sequence.
            if (static_cast<uchar>(bytes.front()) < 0x80)
                break;
            const auto lead = static_cast<uchar>(bytes.front());
            const int need = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : 2;
            if (bytes.size() >= need)
                break;
        }
        if (bytes.size() == 1 && static_cast<uchar>(bytes.front()) < 0x80) {
            const char c = bytes.front();
            if (std::strchr(reserved, c) && c)
                out += s.mid(start, 3);
            else
                out += QLatin1Char(c);
            continue;
        }
        const QString decoded = QString::fromUtf8(bytes);
        if (decoded.contains(QChar(0xfffd)))
            runtime.throwError(ErrorType::URIError, QStringLiteral("malformed URI"));
        out += decoded;
    }
    return out;
}

void installGlobals(Runtime &runtime) {
    Object *g = runtime.global;
    addConstant(g, QStringLiteral("NaN"), Value::number(std::numeric_limits<double>::quiet_NaN()));
    addConstant(g, QStringLiteral("Infinity"), Value::number(std::numeric_limits<double>::infinity()));
    addConstant(g, QStringLiteral("undefined"), Value());
    g->setOwn(QStringLiteral("globalThis"), Value::object(g), false);
    addMethod(runtime, g, QStringLiteral("isNaN"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::boolean(std::isnan(rt.toNumber(argument(args, 0))));
    });
    addMethod(runtime, g, QStringLiteral("isFinite"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::boolean(std::isfinite(rt.toNumber(argument(args, 0))));
    });
    addMethod(runtime, g, QStringLiteral("encodeURIComponent"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  return Value::string(uriEncode(rt, rt.toString(argument(args, 0)), "-_.!~*'()"));
              });
    addMethod(runtime, g, QStringLiteral("encodeURI"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::string(uriEncode(rt, rt.toString(argument(args, 0)), "-_.!~*'();/?:@&=+$,#"));
    });
    addMethod(runtime, g, QStringLiteral("decodeURIComponent"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  return Value::string(uriDecode(rt, rt.toString(argument(args, 0)), ""));
              });
    addMethod(runtime, g, QStringLiteral("decodeURI"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::string(uriDecode(rt, rt.toString(argument(args, 0)), ";/?:@&=+$,#"));
    });
}

} // namespace

void Runtime::installBuiltins() {
    installObject(*this);
    installFunction(*this);
    installArray(*this);
    installBoolean(*this);
    installErrors(*this);
    installMath(*this);
    installGlobals(*this);
    installNumber(*this);
    installStringAndRegExp(*this);
    installDate(*this);
    installJson(*this);
}

} // namespace QtJSPrivate
