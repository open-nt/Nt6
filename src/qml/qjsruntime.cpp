// MIT License - see LICENSE
#include "qjsruntime_p.h"

#include "qjsinterp_p.h"
#include "qjsparser_p.h"

#include <QtCore/qobject.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace QtJSPrivate {

Cell::~Cell() = default;

Value Value::object(Object *o) {
    Value v;
    v.m_type = ObjectType;
    v.m_object = o;
    return v;
}

Function *Value::asFunction() const {
    return m_type == ObjectType && m_object && m_object->cls == Object::Class::Function
               ? static_cast<Function *>(m_object)
               : nullptr;
}

void Value::mark(std::vector<Cell *> *stack) const {
    if (m_type == ObjectType && m_object && !m_object->marked)
        stack->push_back(m_object);
}

bool isArrayIndex(const QString &key, quint32 *index) {
    const qsizetype n = key.size();
    if (n == 0 || n > 10)
        return false;
    if (n > 1 && key.at(0) == u'0')
        return false;
    quint64 v = 0;
    for (const QChar c : key) {
        if (c < u'0' || c > u'9')
            return false;
        v = v * 10 + static_cast<quint64>(c.unicode() - u'0');
    }
    if (v >= 0xffffffffULL)
        return false;
    *index = static_cast<quint32>(v);
    return true;
}

// ----------------------------------------------------------------------------------------------------------------
// Objects

Property *Object::ownProperty(const QString &key) {
    const auto it = m_index.constFind(key);
    return it == m_index.cend() ? nullptr : &m_props[*it].second;
}

const Property *Object::ownProperty(const QString &key) const {
    const auto it = m_index.constFind(key);
    return it == m_index.cend() ? nullptr : &m_props[*it].second;
}

Property &Object::defineOwn(const QString &key) {
    if (Property *p = ownProperty(key))
        return *p;
    m_index.insert(key, m_props.size());
    m_props.emplace_back(key, Property());
    return m_props.back().second;
}

void Object::setOwn(const QString &key, const Value &value, bool enumerable) {
    Property &p = defineOwn(key);
    p.value = value;
    p.accessor = false;
    p.getter = p.setter = nullptr;
    p.enumerable = enumerable;
}

bool Object::removeOwn(const QString &key) {
    const auto it = m_index.find(key);
    if (it == m_index.end())
        return false;
    const std::size_t at = *it;
    m_props.erase(m_props.begin() + static_cast<std::ptrdiff_t>(at));
    m_index.erase(it);
    for (auto &i : m_index)
        if (i > at)
            --i;
    return true;
}

QStringList Object::ownPropertyKeys() const {
    QStringList keys;
    keys.reserve(static_cast<qsizetype>(m_props.size()));
    for (const auto &p : m_props)
        keys.append(p.first);
    return keys;
}

bool Object::getOwn(Runtime &rt, const QString &key, const Value &receiver, Value *out) {
    quint32 index = 0;
    if (cls == Class::Array || cls == Class::Arguments) {
        if (isArrayIndex(key, &index)) {
            if (index < elements.size()) {
                *out = elements[index];
                return true;
            }
        } else if (key == QLatin1String("length")) {
            *out = Value::number(static_cast<double>(elements.size()));
            return true;
        }
    } else if (cls == Class::String) {
        const QString &s = primitive.asString();
        if (key == QLatin1String("length")) {
            *out = Value::number(static_cast<double>(s.size()));
            return true;
        }
        if (isArrayIndex(key, &index) && index < static_cast<quint32>(s.size())) {
            *out = Value::string(QString(s.at(index)));
            return true;
        }
    }
    const Property *p = ownProperty(key);
    if (!p)
        return false;
    if (p->accessor)
        *out = p->getter ? rt.call(Value::object(p->getter), receiver, {}) : Value();
    else
        *out = p->value;
    return true;
}

bool Object::setOwn(Runtime &rt, const QString &key, const Value &value, const Value &receiver) {
    (void)receiver;
    if (frozen)
        return false;
    quint32 index = 0;
    if (cls == Class::Array || cls == Class::Arguments) {
        if (isArrayIndex(key, &index)) {
            // Arrays have no holes: writing past the end fills the gap with undefined, within a limit.
            if (index >= elements.size()) {
                if (index > elements.size() + 10000000u)
                    rt.throwError(ErrorType::RangeError, QStringLiteral("array index too large"));
                elements.resize(index + 1);
            }
            elements[index] = value;
            return true;
        }
        if (key == QLatin1String("length")) {
            const double n = rt.toNumber(value);
            if (!(n >= 0) || n != std::floor(n) || n > 4294967295.0)
                rt.throwError(ErrorType::RangeError, QStringLiteral("invalid array length"));
            if (n > static_cast<double>(elements.size()) + 10000000.0)
                rt.throwError(ErrorType::RangeError, QStringLiteral("array length too large"));
            elements.resize(static_cast<std::size_t>(n));
            return true;
        }
    } else if (cls == Class::String) {
        if (key == QLatin1String("length") || (isArrayIndex(key, &index) && index < primitive.asString().size()))
            return false;
    }
    Property *p = ownProperty(key);
    if (p) {
        if (p->accessor) {
            if (p->setter)
                rt.call(Value::object(p->setter), receiver, {value});
            return p->setter != nullptr;
        }
        if (!p->writable)
            return false;
        p->value = value;
        return true;
    }
    if (!extensible)
        return false;
    setOwn(key, value);
    return true;
}

bool Object::hasOwn(Runtime &rt, const QString &key) {
    (void)rt;
    quint32 index = 0;
    if (cls == Class::Array || cls == Class::Arguments) {
        if (isArrayIndex(key, &index))
            return index < elements.size();
        if (key == QLatin1String("length"))
            return true;
    } else if (cls == Class::String) {
        if (key == QLatin1String("length"))
            return true;
        if (isArrayIndex(key, &index))
            return index < static_cast<quint32>(primitive.asString().size());
    }
    return ownProperty(key) != nullptr;
}

bool Object::deleteOwn(Runtime &rt, const QString &key) {
    (void)rt;
    if (frozen)
        return false;
    quint32 index = 0;
    if ((cls == Class::Array || cls == Class::Arguments) && isArrayIndex(key, &index)) {
        if (index < elements.size()) {
            if (index + 1 == elements.size())
                elements.pop_back();
            else
                elements[index] = Value();
        }
        return true;
    }
    const Property *p = ownProperty(key);
    if (!p)
        return true;
    if (!p->configurable)
        return false;
    return removeOwn(key);
}

QStringList Object::enumerableKeys(Runtime &rt) {
    (void)rt;
    QStringList keys;
    if (cls == Class::Array || cls == Class::Arguments) {
        for (std::size_t i = 0; i < elements.size(); ++i)
            keys.append(QString::number(i));
    } else if (cls == Class::String) {
        for (qsizetype i = 0; i < primitive.asString().size(); ++i)
            keys.append(QString::number(i));
    }
    for (const auto &p : m_props)
        if (p.second.enumerable)
            keys.append(p.first);
    return keys;
}

void Object::markChildren(std::vector<Cell *> *stack) const {
    if (proto && !proto->marked)
        stack->push_back(proto);
    primitive.mark(stack);
    for (const Value &v : elements)
        v.mark(stack);
    for (const auto &p : m_props) {
        p.second.value.mark(stack);
        if (p.second.getter && !p.second.getter->marked)
            stack->push_back(p.second.getter);
        if (p.second.setter && !p.second.setter->marked)
            stack->push_back(p.second.setter);
    }
}

void Function::markChildren(std::vector<Cell *> *stack) const {
    Object::markChildren(stack);
    if (scope && !scope->marked)
        stack->push_back(scope);
    if (target && !target->marked)
        stack->push_back(target);
    boundThis.mark(stack);
    for (const Value &v : boundArgs)
        v.mark(stack);
}

void Env::markChildren(std::vector<Cell *> *stack) const {
    if (parent && !parent->marked)
        stack->push_back(parent);
    if (object && !object->marked)
        stack->push_back(object);
    if (callee && !callee->marked)
        stack->push_back(callee);
    thisValue.mark(stack);
    for (const Value &v : arguments)
        v.mark(stack);
    for (const Binding &b : vars)
        b.value.mark(stack);
}

// ----------------------------------------------------------------------------------------------------------------
// Heap

Heap::~Heap() {
    for (Cell *c : m_cells)
        delete c;
}

void Heap::collect(const std::vector<Cell *> &roots, const std::function<void(Cell *)> &beforeFree) {
    std::vector<Cell *> stack;
    for (Cell *r : roots)
        if (r && !r->marked)
            stack.push_back(r);
    while (!stack.empty()) {
        Cell *c = stack.back();
        stack.pop_back();
        if (c->marked)
            continue;
        c->marked = true;
        c->markChildren(&stack);
    }
    std::vector<Cell *> live;
    std::vector<Cell *> dead;
    live.reserve(m_cells.size());
    for (Cell *c : m_cells) {
        if (c->marked) {
            c->marked = false;
            live.push_back(c);
        } else {
            dead.push_back(c);
        }
    }
    m_cells.swap(live);
    for (Cell *c : dead)
        if (beforeFree)
            beforeFree(c);
    for (Cell *c : dead)
        delete c;
    m_sinceCollect = 0;
}

// ----------------------------------------------------------------------------------------------------------------
// Runtime

DepthGuard::DepthGuard(Runtime &r) : rt(r) { ++rt.depth; }

Runtime::Runtime(QJSEngine *e) : engine(e) {
    objectProto = heap.make<Object>(nullptr);
    functionProto = heap.make<Function>(objectProto);
    functionProto->native = [](Runtime &, const Value &, const Values &, Object *) { return Value(); };
    functionProto->constructor = false;
    global = heap.make<Object>(objectProto);
    globalEnv = heap.make<Env>(nullptr);
    globalEnv->object = global;
    globalEnv->hasThis = true;
    globalEnv->thisValue = Value::object(global);
    installBuiltins();
    installQObjectSupport();
}

Runtime::~Runtime() = default;

Object *Runtime::newArray(const Values &elements) {
    Object *a = heap.make<Object>(arrayProto, Object::Class::Array);
    a->elements = elements;
    return a;
}

Function *Runtime::newNative(const QString &name, int length, Function::Native fn, bool constructor) {
    Function *f = heap.make<Function>(functionProto);
    f->native = std::move(fn);
    f->constructor = constructor;
    f->name = name;
    Property &n = f->defineOwn(QStringLiteral("name"));
    n.value = Value::string(name);
    n.enumerable = n.writable = false;
    Property &l = f->defineOwn(QStringLiteral("length"));
    l.value = Value::number(length);
    l.enumerable = l.writable = false;
    return f;
}

Object *Runtime::newError(ErrorType type, const QString &message) {
    Object *e = heap.make<Object>(errorProtos[static_cast<int>(type)], Object::Class::Error);
    e->setOwn(QStringLiteral("message"), Value::string(message), false);
    if (!currentFile.isEmpty())
        e->setOwn(QStringLiteral("fileName"), Value::string(currentFile), false);
    if (currentLine > 0)
        e->setOwn(QStringLiteral("lineNumber"), Value::number(currentLine), false);
    e->setOwn(QStringLiteral("stack"), Value::string(stack.join(u'\n')), false);
    return e;
}

void Runtime::throwError(ErrorType type, const QString &message) {
    throw Throw{Value::object(newError(type, message))};
}

bool Runtime::toBoolean(const Value &v) const {
    switch (v.type()) {
    case Value::Undefined:
    case Value::Null:
        return false;
    case Value::Boolean:
        return v.asBoolean();
    case Value::Number:
        return v.asNumber() != 0.0 && !std::isnan(v.asNumber());
    case Value::String:
        return !v.asString().isEmpty();
    case Value::ObjectType:
        return true;
    }
    return false;
}

double Runtime::stringToNumber(const QString &input) {
    // White space as JavaScript defines it, including line terminators.
    qsizetype b = 0, e = input.size();
    const auto space = [](QChar c) { return c.isSpace() || c == QChar(0xfeff); };
    while (b < e && space(input.at(b)))
        ++b;
    while (e > b && space(input.at(e - 1)))
        --e;
    const QString s = input.mid(b, e - b);
    if (s.isEmpty())
        return 0.0;
    if (s.size() > 2 && s.at(0) == u'0') {
        const QChar k = s.at(1).toLower();
        const int base = k == u'x' ? 16 : k == u'o' ? 8 : k == u'b' ? 2 : 0;
        if (base) {
            double v = 0.0;
            for (qsizetype i = 2; i < s.size(); ++i) {
                const QChar c = s.at(i).toLower();
                int d = -1;
                if (c >= u'0' && c <= u'9')
                    d = c.unicode() - u'0';
                else if (c >= u'a' && c <= u'z')
                    d = c.unicode() - u'a' + 10;
                if (d < 0 || d >= base)
                    return std::numeric_limits<double>::quiet_NaN();
                v = v * base + d;
            }
            return v;
        }
    }
    const QString &t = s;
    if (t == QLatin1String("Infinity") || t == QLatin1String("+Infinity"))
        return std::numeric_limits<double>::infinity();
    if (t == QLatin1String("-Infinity"))
        return -std::numeric_limits<double>::infinity();
    // A decimal literal: sign, digits, an optional fraction and exponent; nothing else.
    qsizetype i = 0;
    if (i < t.size() && (t.at(i) == u'+' || t.at(i) == u'-'))
        ++i;
    qsizetype digits = 0;
    while (i < t.size() && t.at(i).isDigit() && t.at(i).unicode() < 128) {
        ++i;
        ++digits;
    }
    if (i < t.size() && t.at(i) == u'.') {
        ++i;
        while (i < t.size() && t.at(i).isDigit() && t.at(i).unicode() < 128) {
            ++i;
            ++digits;
        }
    }
    if (digits == 0)
        return std::numeric_limits<double>::quiet_NaN();
    if (i < t.size() && (t.at(i) == u'e' || t.at(i) == u'E')) {
        ++i;
        if (i < t.size() && (t.at(i) == u'+' || t.at(i) == u'-'))
            ++i;
        qsizetype exp = 0;
        while (i < t.size() && t.at(i).isDigit() && t.at(i).unicode() < 128) {
            ++i;
            ++exp;
        }
        if (exp == 0)
            return std::numeric_limits<double>::quiet_NaN();
    }
    if (i != t.size())
        return std::numeric_limits<double>::quiet_NaN();
    const QByteArray latin = t.toLatin1();
    return std::strtod(latin.constData(), nullptr);
}

QString Runtime::numberToString(double d) {
    if (std::isnan(d))
        return QStringLiteral("NaN");
    if (d == 0.0)
        return QStringLiteral("0");
    if (std::isinf(d))
        return d > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
    if (d < 0)
        return u'-' + numberToString(-d);
    // The shortest digits that read back as the same number.
    char buffer[64];
    int precision = 1;
    for (; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof buffer, "%.*e", precision - 1, d);
        if (std::strtod(buffer, nullptr) == d)
            break;
    }
    QString digits;
    const char *p = buffer;
    for (; *p && *p != 'e'; ++p)
        if (*p != '.')
            digits += QLatin1Char(*p);
    const int exponent = std::atoi(p + 1);
    while (digits.size() > 1 && digits.endsWith(u'0'))
        digits.chop(1);
    const int k = static_cast<int>(digits.size());
    const int n = exponent + 1;
    if (k <= n && n <= 21)
        return digits + QString(n - k, u'0');
    if (0 < n && n <= 21)
        return digits.left(n) + u'.' + digits.mid(n);
    if (-6 < n && n <= 0)
        return QStringLiteral("0.") + QString(-n, u'0') + digits;
    QString out = digits.left(1);
    if (k > 1)
        out += u'.' + digits.mid(1);
    out += u'e';
    out += n - 1 >= 0 ? u'+' : u'-';
    out += QString::number(std::abs(n - 1));
    return out;
}

double Runtime::toNumber(const Value &v) {
    switch (v.type()) {
    case Value::Undefined:
        return std::numeric_limits<double>::quiet_NaN();
    case Value::Null:
        return 0.0;
    case Value::Boolean:
        return v.asBoolean() ? 1.0 : 0.0;
    case Value::Number:
        return v.asNumber();
    case Value::String:
        return stringToNumber(v.asString());
    case Value::ObjectType:
        return toNumber(toPrimitive(v, false));
    }
    return 0.0;
}

QString Runtime::toString(const Value &v) {
    switch (v.type()) {
    case Value::Undefined:
        return QStringLiteral("undefined");
    case Value::Null:
        return QStringLiteral("null");
    case Value::Boolean:
        return v.asBoolean() ? QStringLiteral("true") : QStringLiteral("false");
    case Value::Number:
        return numberToString(v.asNumber());
    case Value::String:
        return v.asString();
    case Value::ObjectType:
        return toString(toPrimitive(v, true));
    }
    return QString();
}

Value Runtime::toPrimitive(const Value &v, bool preferString) {
    if (!v.isObject())
        return v;
    Object *o = v.asObject();
    if (o->cls == Object::Class::Date)
        preferString = true;
    const QString first = preferString ? QStringLiteral("toString") : QStringLiteral("valueOf");
    const QString second = preferString ? QStringLiteral("valueOf") : QStringLiteral("toString");
    for (const QString &name : {first, second}) {
        const Value fn = getFromObject(o, name, v);
        if (isCallable(fn)) {
            const Value r = call(fn, v, {});
            if (!r.isObject())
                return r;
        }
    }
    throwTypeError(QStringLiteral("cannot convert object to primitive value"));
}

Object *Runtime::toObject(const Value &v) {
    switch (v.type()) {
    case Value::Undefined:
    case Value::Null:
        throwTypeError(QStringLiteral("cannot convert %1 to object").arg(toString(v)));
    case Value::Boolean: {
        Object *o = heap.make<Object>(booleanProto, Object::Class::Boolean);
        o->primitive = v;
        return o;
    }
    case Value::Number: {
        Object *o = heap.make<Object>(numberProto, Object::Class::Number);
        o->primitive = v;
        return o;
    }
    case Value::String: {
        Object *o = heap.make<Object>(stringProto, Object::Class::String);
        o->primitive = v;
        return o;
    }
    case Value::ObjectType:
        return v.asObject();
    }
    return nullptr;
}

QString Runtime::typeOf(const Value &v) const {
    switch (v.type()) {
    case Value::Undefined:
        return QStringLiteral("undefined");
    case Value::Null:
        return QStringLiteral("object");
    case Value::Boolean:
        return QStringLiteral("boolean");
    case Value::Number:
        return QStringLiteral("number");
    case Value::String:
        return QStringLiteral("string");
    case Value::ObjectType:
        return v.asFunction() ? QStringLiteral("function") : QStringLiteral("object");
    }
    return QString();
}

Value Runtime::getFromObject(Object *object, const QString &key, const Value &receiver) {
    int guard = 0;
    for (Object *o = object; o; o = o->proto) {
        Value v;
        if (o->getOwn(*this, key, receiver, &v))
            return v;
        if (++guard > 10000)
            throwError(ErrorType::RangeError, QStringLiteral("prototype chain too long"));
    }
    return Value();
}

Value Runtime::get(const Value &base, const QString &key) {
    switch (base.type()) {
    case Value::ObjectType:
        return getFromObject(base.asObject(), key, base);
    case Value::String: {
        const QString &s = base.asString();
        if (key == QLatin1String("length"))
            return Value::number(static_cast<double>(s.size()));
        quint32 index = 0;
        if (isArrayIndex(key, &index))
            return index < static_cast<quint32>(s.size()) ? Value::string(QString(s.at(index))) : Value();
        return getFromObject(stringProto, key, base);
    }
    case Value::Number:
        return getFromObject(numberProto, key, base);
    case Value::Boolean:
        return getFromObject(booleanProto, key, base);
    case Value::Undefined:
    case Value::Null:
        throwTypeError(QStringLiteral("cannot read property '%1' of %2").arg(key, toString(base)));
    }
    return Value();
}

bool Runtime::putToObject(Object *object, const QString &key, const Value &value, const Value &receiver) {
    // An accessor or a read-only property further up the chain decides; otherwise the receiver gets it.
    int guard = 0;
    for (Object *o = object->proto; o; o = o->proto) {
        if (o->cls == Object::Class::Host)
            break;
        if (const Property *p = o->ownProperty(key)) {
            if (object->ownProperty(key))
                break;
            if (p->accessor) {
                if (p->setter)
                    call(Value::object(p->setter), receiver, {value});
                return p->setter != nullptr;
            }
            if (!p->writable)
                return false;
            break;
        }
        if (++guard > 10000)
            break;
    }
    return object->setOwn(*this, key, value, receiver);
}

void Runtime::put(const Value &base, const QString &key, const Value &value) {
    if (base.isObject()) {
        putToObject(base.asObject(), key, value, base);
        return;
    }
    if (base.isNullish())
        throwTypeError(QStringLiteral("cannot set property '%1' of %2").arg(key, toString(base)));
    // Writing to a primitive does nothing.
}

bool Runtime::has(Object *object, const QString &key) {
    for (Object *o = object; o; o = o->proto)
        if (o->hasOwn(*this, key))
            return true;
    return false;
}

Value Runtime::call(const Value &function, const Value &thisValue, const Values &args) {
    Function *f = function.asFunction();
    if (!f)
        throwTypeError(QStringLiteral("%1 is not a function")
                           .arg(function.isObject() ? QStringLiteral("object") : toString(function)));
    if (callDepth > 300)
        throwError(ErrorType::RangeError, QStringLiteral("maximum call stack size exceeded"));
    ++callDepth;
    struct Leave {
        int *d;
        ~Leave() { --*d; }
    } leave{&callDepth};
    if (f->target) {
        Values all = f->boundArgs;
        all.insert(all.end(), args.begin(), args.end());
        return call(Value::object(f->target), f->boundThis, all);
    }
    if (f->native)
        return f->native(*this, thisValue, args, nullptr);
    return invokeScript(*this, f, thisValue, args, nullptr);
}

Value Runtime::construct(const Value &function, const Values &args, Object *newTarget) {
    Function *f = function.asFunction();
    if (!f || !f->constructor)
        throwTypeError(QStringLiteral("%1 is not a constructor").arg(f ? f->name : toString(function)));
    if (callDepth > 300)
        throwError(ErrorType::RangeError, QStringLiteral("maximum call stack size exceeded"));
    ++callDepth;
    struct Leave {
        int *d;
        ~Leave() { --*d; }
    } leave{&callDepth};
    if (f->target) {
        Values all = f->boundArgs;
        all.insert(all.end(), args.begin(), args.end());
        return construct(Value::object(f->target), all, newTarget == f ? f->target : newTarget);
    }
    Object *target = newTarget ? newTarget : f;
    if (f->native)
        return f->native(*this, Value(), args, target);
    const Value protoValue = getFromObject(target, QStringLiteral("prototype"), Value::object(target));
    Object *object = heap.make<Object>(protoValue.isObject() ? protoValue.asObject() : objectProto);
    const Value self = Value::object(object);
    const Value r = invokeScript(*this, f, self, args, target);
    return r.isObject() ? r : self;
}

bool Runtime::strictEquals(const Value &a, const Value &b) const {
    if (a.type() != b.type())
        return false;
    switch (a.type()) {
    case Value::Undefined:
    case Value::Null:
        return true;
    case Value::Boolean:
        return a.asBoolean() == b.asBoolean();
    case Value::Number:
        return a.asNumber() == b.asNumber();
    case Value::String:
        return a.asString() == b.asString();
    case Value::ObjectType:
        return a.asObject() == b.asObject();
    }
    return false;
}

bool Runtime::sameValueZero(const Value &a, const Value &b) const {
    if (a.isNumber() && b.isNumber() && std::isnan(a.asNumber()) && std::isnan(b.asNumber()))
        return true;
    return strictEquals(a, b);
}

bool Runtime::looseEquals(const Value &a, const Value &b) {
    if (a.type() == b.type())
        return strictEquals(a, b);
    if (a.isNullish() && b.isNullish())
        return true;
    if (a.isNullish() || b.isNullish())
        return false;
    if (a.isNumber() && b.isString())
        return a.asNumber() == stringToNumber(b.asString());
    if (a.isString() && b.isNumber())
        return stringToNumber(a.asString()) == b.asNumber();
    if (a.isBoolean())
        return looseEquals(Value::number(a.asBoolean() ? 1 : 0), b);
    if (b.isBoolean())
        return looseEquals(a, Value::number(b.asBoolean() ? 1 : 0));
    if ((a.isNumber() || a.isString()) && b.isObject())
        return looseEquals(a, toPrimitive(b, false));
    if (a.isObject() && (b.isNumber() || b.isString()))
        return looseEquals(toPrimitive(a, false), b);
    return false;
}

Value Runtime::evaluate(const QString &source, const QString &fileName, int line, Env *scope, const Value &thisValue) {
    ParseError error;
    const std::shared_ptr<ProgramData> program = Parser::parseScript(source, fileName, line, &error);
    if (!program) {
        const QString file = currentFile;
        const int l = currentLine;
        currentFile = fileName;
        currentLine = error.line;
        Object *e = newError(ErrorType::SyntaxError, error.message);
        currentFile = file;
        currentLine = l;
        throw Throw{Value::object(e)};
    }
    return runProgram(*this, program, scope ? scope : globalEnv, thisValue);
}

void Runtime::maybeCollect() {
    if (depth == 0 && callDepth == 0 && heap.sinceCollect() > 20000)
        collect();
}

void Runtime::collect() {
    if (depth != 0 || callDepth != 0)
        return;
    std::vector<Cell *> roots = {global,      globalEnv,    objectProto, functionProto, arrayProto, stringProto,
                                 numberProto, booleanProto, dateProto,   regexpProto,   errorProto, qobjectProto};
    for (Object *p : errorProtos)
        roots.push_back(p);
    for (auto it = persistent.cbegin(); it != persistent.cend(); ++it)
        roots.push_back(it.key());
    for (const auto &provider : rootProviders)
        provider(&roots);
    // Wrappers of objects that are alive and owned by C++ stay, so that properties set on them by scripts
    // are kept.
    for (auto it = wrappers.cbegin(); it != wrappers.cend(); ++it)
        if (!scriptOwned.value(it.key()))
            roots.push_back(it.value());
    for (const ScriptConnection &c : connections) {
        if (c.function)
            roots.push_back(c.function);
        if (c.thisObject.isObject())
            roots.push_back(c.thisObject.asObject());
    }
    std::vector<QObject *> doomed;
    heap.collect(roots, [this, &doomed](Cell *cell) {
        auto *w = dynamic_cast<QObjectWrapper *>(cell);
        if (!w || wrappers.value(w->key) != w)
            return;
        QObject *qo = w->object.data();
        if (qo && scriptOwned.value(w->key) && !qo->parent())
            doomed.push_back(qo);
        scriptOwned.remove(w->key);
        wrappers.remove(w->key);
    });
    for (QObject *o : doomed)
        delete o;
}

} // namespace QtJSPrivate
