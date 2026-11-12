// MIT License - see LICENSE
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsvalueiterator.h>

#include "qjsengine_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qobject.h>

#include <cmath>

using QtJSPrivate::EngineAccess;
using QtJSPrivate::Object;
using QtJSPrivate::Runtime;
using QtJSPrivate::Value;

QJSValuePrivate::QJSValuePrivate(Runtime *runtime, const Value &v) : rt(runtime), value(v) {
    if (!rt)
        return;
    rt->handles.insert(this);
    if (value.isObject())
        ++rt->persistent[value.asObject()];
}

QJSValuePrivate::~QJSValuePrivate() { release(); }

void QJSValuePrivate::release() {
    if (!rt)
        return;
    rt->handles.remove(this);
    if (value.isObject()) {
        const auto it = rt->persistent.find(value.asObject());
        if (it != rt->persistent.end() && --*it <= 0)
            rt->persistent.erase(it);
    }
    rt = nullptr;
}

namespace QtJSPrivate {

QJSValue EngineAccess::make(Runtime *rt, const Value &v) {
    // Primitives need no engine.
    return QJSValue(std::make_shared<QJSValuePrivate>(v.isObject() ? rt : nullptr, v));
}

Value EngineAccess::value(Runtime *rt, const QJSValue &v) {
    const QJSValuePrivate *d = v.d.get();
    if (!d)
        return Value();
    if (!d->value.isObject())
        return d->value;
    return d->rt == rt ? d->value : Value();
}

} // namespace QtJSPrivate

namespace {

Runtime *runtimeOf(const QJSValuePrivate *d) { return d ? d->rt : nullptr; }

// Runs an operation of an object value; errors thrown by scripts become the result.
template <typename F> QJSValue run(Runtime *rt, F f) { return EngineAccess::guarded(rt, f); }

} // namespace

/*!
    \class QJSValue
    \brief The QJSValue class holds a value of a JavaScript engine.

    A QJSValue is a primitive (undefined, null, a boolean, a number or a string) or a reference to an object
    of a QJSEngine. Copies refer to the same object, and keep it alive.

    \code
    QJSValue point = engine.newObject();
    point.setProperty("x", 3);
    point.setProperty("y", 4);
    QJSValue length = engine.evaluate("(function(p) { return Math.hypot(p.x, p.y); })");
    qDebug() << length.call({point}).toNumber();   // 5
    \endcode

    Values made without an engine, such as \c{QJSValue(42)}, are primitives; they can be passed to any
    engine. Objects belong to the engine that made them; after the engine is destroyed they read as
    undefined.

    Calling a function that throws returns the error: check isError(). Arguments are passed as a
    \c QJSValueList, a QList of QJSValue.

    \differences
    There is no \c toPrimitive(), \c toQMetaObject() or \c isQMetaObject(). toVariant() with
    RetainJSObjects converts objects as ConvertJSObjects does.
    \enddifferences

    \sa QJSEngine
*/

/*!
    \enum QJSValue::SpecialValue

    \value NullValue A null value.
    \value UndefinedValue An undefined value.
*/

/*!
    \enum QJSValue::ErrorType

    The type of an error object.

    \value NoError Not an error.
    \value GenericError An Error.
    \value EvalError An EvalError.
    \value RangeError A RangeError.
    \value ReferenceError A ReferenceError.
    \value SyntaxError A SyntaxError.
    \value TypeError A TypeError.
    \value URIError A URIError.
*/

/*!
    \enum QJSValue::ObjectConversionBehavior

    How toVariant() converts objects.

    \value ConvertJSObjects Arrays become QVariantList and objects QVariantMap.
    \value RetainJSObjects The same in Nt.
*/

/*!
    Constructs \a value: undefined or null.
*/
QJSValue::QJSValue(SpecialValue value)
    : d(std::make_shared<QJSValuePrivate>(nullptr, value == NullValue ? Value::null() : Value())) {}

/*!
    Constructs the boolean \a value.
*/
QJSValue::QJSValue(bool value) : d(std::make_shared<QJSValuePrivate>(nullptr, Value::boolean(value))) {}

/*!
    Constructs the number \a value.
*/
QJSValue::QJSValue(int value) : d(std::make_shared<QJSValuePrivate>(nullptr, Value::number(value))) {}

/*!
    Constructs the number \a value.
*/
QJSValue::QJSValue(uint value) : d(std::make_shared<QJSValuePrivate>(nullptr, Value::number(value))) {}

/*!
    Constructs the number \a value.
*/
QJSValue::QJSValue(double value) : d(std::make_shared<QJSValuePrivate>(nullptr, Value::number(value))) {}

/*!
    Constructs the string \a value.
*/
QJSValue::QJSValue(const QString &value) : d(std::make_shared<QJSValuePrivate>(nullptr, Value::string(value))) {}

/*!
    Constructs the string \a value.
*/
QJSValue::QJSValue(QLatin1String value)
    : d(std::make_shared<QJSValuePrivate>(nullptr, Value::string(QString(value)))) {}

/*!
    Constructs the string \a str, read as UTF-8.
*/
QJSValue::QJSValue(const char *str)
    : d(std::make_shared<QJSValuePrivate>(nullptr, Value::string(QString::fromUtf8(str)))) {}

/*!
    Constructs a copy of \a other, which refers to the same object.
*/
QJSValue::QJSValue(const QJSValue &other) = default;

/*!
    Constructs a value by moving \a other.
*/
QJSValue::QJSValue(QJSValue &&other) noexcept = default;

/*!
    Makes this value refer to what \a other refers to.
*/
QJSValue &QJSValue::operator=(const QJSValue &other) = default;

/*!
    Moves \a other into this value.
*/
QJSValue &QJSValue::operator=(QJSValue &&other) noexcept = default;

/*!
    Destroys the value. The object it refers to may be collected once nothing else refers to it.
*/
QJSValue::~QJSValue() = default;

QJSValue::QJSValue(std::shared_ptr<QJSValuePrivate> data) : d(std::move(data)) {}

/*!
    \fn void QJSValue::swap(QJSValue &other)

    Swaps this value with \a other.
*/

/*!
    Returns \c true if the value is a boolean; otherwise returns \c false.
*/
bool QJSValue::isBool() const { return d && d->value.isBoolean(); }

/*!
    Returns \c true if the value is a number; otherwise returns \c false.
*/
bool QJSValue::isNumber() const { return d && d->value.isNumber(); }

/*!
    Returns \c true if the value is null; otherwise returns \c false.
*/
bool QJSValue::isNull() const { return d && d->value.isNull(); }

/*!
    Returns \c true if the value is a string; otherwise returns \c false.
*/
bool QJSValue::isString() const { return d && d->value.isString(); }

/*!
    Returns \c true if the value is undefined; otherwise returns \c false.
*/
bool QJSValue::isUndefined() const { return !d || d->value.isUndefined(); }

/*!
    Returns \c true if the value holds a C++ value that has no JavaScript form; otherwise returns \c false.
*/
bool QJSValue::isVariant() const {
    return isObject() && d->value.asObject()->cls == Object::Class::Host && d->value.asObject()->variant.isValid();
}

/*!
    Returns \c true if the value is a QObject; otherwise returns \c false.

    \sa toQObject()
*/
bool QJSValue::isQObject() const { return toQObject() != nullptr; }

/*!
    Returns \c true if the value is an object, functions and arrays included; otherwise returns \c false.
*/
bool QJSValue::isObject() const { return d && d->rt && d->value.isObject(); }

/*!
    Returns \c true if the value is a Date; otherwise returns \c false.
*/
bool QJSValue::isDate() const { return isObject() && d->value.asObject()->cls == Object::Class::Date; }

/*!
    Returns \c true if the value is a RegExp; otherwise returns \c false.
*/
bool QJSValue::isRegExp() const { return isObject() && d->value.asObject()->cls == Object::Class::RegExp; }

/*!
    Returns \c true if the value is an array; otherwise returns \c false.
*/
bool QJSValue::isArray() const { return isObject() && d->value.asObject()->cls == Object::Class::Array; }

/*!
    Returns \c true if the value is an error; otherwise returns \c false.
*/
bool QJSValue::isError() const { return isObject() && d->value.asObject()->cls == Object::Class::Error; }

/*!
    Returns \c true if the value is a URL; otherwise returns \c false. URLs are strings in Nt, so this is
    always \c false.
*/
bool QJSValue::isUrl() const { return false; }

/*!
    Returns \c true if the value is a function; otherwise returns \c false.
*/
bool QJSValue::isCallable() const { return isObject() && d->value.asFunction(); }

/*!
    Returns the value converted to a string, as JavaScript's \c String(value) does. An error while
    converting gives the text of the error.
*/
QString QJSValue::toString() const {
    if (!d)
        return QStringLiteral("undefined");
    Runtime *rt = runtimeOf(d.get());
    if (!rt) {
        const Value &v = d->value;
        switch (v.type()) {
        case Value::Null:
            return QStringLiteral("null");
        case Value::Boolean:
            return v.asBoolean() ? QStringLiteral("true") : QStringLiteral("false");
        case Value::Number:
            return Runtime::numberToString(v.asNumber());
        case Value::String:
            return v.asString();
        default:
            return QStringLiteral("undefined");
        }
    }
    try {
        return rt->toString(d->value);
    } catch (const QtJSPrivate::Throw &t) {
        try {
            return rt->toString(t.value);
        } catch (const QtJSPrivate::Throw &) {
            return QStringLiteral("Error");
        }
    }
}

/*!
    Returns the value converted to a number, as JavaScript's \c Number(value) does.
*/
double QJSValue::toNumber() const {
    if (!d)
        return std::numeric_limits<double>::quiet_NaN();
    const Value &v = d->value;
    switch (v.type()) {
    case Value::Undefined:
        return std::numeric_limits<double>::quiet_NaN();
    case Value::Null:
        return 0;
    case Value::Boolean:
        return v.asBoolean() ? 1 : 0;
    case Value::Number:
        return v.asNumber();
    case Value::String:
        return Runtime::stringToNumber(v.asString());
    case Value::ObjectType:
        break;
    }
    Runtime *rt = runtimeOf(d.get());
    if (!rt)
        return std::numeric_limits<double>::quiet_NaN();
    try {
        return rt->toNumber(v);
    } catch (const QtJSPrivate::Throw &) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

/*!
    Returns the value converted to a 32-bit integer, as JavaScript's \c{value | 0} does.
*/
qint32 QJSValue::toInt() const { return static_cast<qint32>(toUInt()); }

/*!
    Returns the value converted to a 32-bit unsigned integer, as JavaScript's \c{value >>> 0} does.
*/
quint32 QJSValue::toUInt() const {
    const double n = toNumber();
    if (!std::isfinite(n))
        return 0;
    double m = std::fmod(std::trunc(n), 4294967296.0);
    if (m < 0)
        m += 4294967296.0;
    return static_cast<quint32>(m);
}

/*!
    Returns the value converted to a boolean, as JavaScript's \c Boolean(value) does.
*/
bool QJSValue::toBool() const {
    if (!d)
        return false;
    const Value &v = d->value;
    switch (v.type()) {
    case Value::Boolean:
        return v.asBoolean();
    case Value::Number:
        return v.asNumber() != 0 && !std::isnan(v.asNumber());
    case Value::String:
        return !v.asString().isEmpty();
    case Value::ObjectType:
        return d->rt != nullptr;
    default:
        return false;
    }
}

/*!
    Returns the value as a QVariant: booleans, numbers (as double) and strings as such, arrays as
    QVariantList, dates as QDateTime, QObjects as \c{QObject *}, other objects as QVariantMap, and null as
    a QVariant holding \c nullptr.
*/
QVariant QJSValue::toVariant() const {
    if (!d)
        return QVariant();
    Runtime *rt = runtimeOf(d.get());
    if (!rt) {
        const Value &v = d->value;
        switch (v.type()) {
        case Value::Null:
            return QVariant::fromValue(nullptr);
        case Value::Boolean:
            return v.asBoolean();
        case Value::Number:
            return v.asNumber();
        case Value::String:
            return v.asString();
        default:
            return QVariant();
        }
    }
    try {
        return rt->toVariant(d->value);
    } catch (const QtJSPrivate::Throw &) {
        return QVariant();
    }
}

/*!
    \overload

    Returns the value as a QVariant; \a behavior is not used.
*/
QVariant QJSValue::toVariant(ObjectConversionBehavior behavior) const {
    (void)behavior;
    return toVariant();
}

/*!
    Returns the QObject the value refers to, or \c nullptr if it is not a QObject or the object was deleted.
*/
QObject *QJSValue::toQObject() const {
    if (!isObject())
        return nullptr;
    return d->rt->unwrapQObject(d->value);
}

/*!
    Returns the date the value holds, or an invalid QDateTime if it is not a Date.
*/
QDateTime QJSValue::toDateTime() const {
    if (!isDate())
        return QDateTime();
    const double t = d->value.asObject()->primitive.asNumber();
    return std::isnan(t) ? QDateTime() : QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t));
}

/*!
    Returns \c true if the value equals \a other with JavaScript's \c == operator; otherwise returns \c false.
*/
bool QJSValue::equals(const QJSValue &other) const {
    Runtime *rt = runtimeOf(d.get()) ? runtimeOf(d.get()) : runtimeOf(other.d.get());
    const Value a = d ? d->value : Value(), b = other.d ? other.d->value : Value();
    if (!rt)
        return strictlyEquals(other) || (a.isNullish() && b.isNullish()) ||
               (!a.isObject() && !b.isObject() && toNumber() == other.toNumber() && !a.isNullish() && !b.isNullish());
    try {
        return rt->looseEquals(a, b);
    } catch (const QtJSPrivate::Throw &) {
        return false;
    }
}

/*!
    Returns \c true if the value equals \a other with JavaScript's \c === operator; otherwise returns
    \c false.
*/
bool QJSValue::strictlyEquals(const QJSValue &other) const {
    const Value a = d ? d->value : Value(), b = other.d ? other.d->value : Value();
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
        return a.asObject() == b.asObject() && d->rt == other.d->rt;
    }
    return false;
}

/*!
    Returns the prototype of the object, or null.
*/
QJSValue QJSValue::prototype() const {
    if (!isObject())
        return QJSValue();
    Object *p = d->value.asObject()->proto;
    return EngineAccess::make(d->rt, p ? Value::object(p) : Value::null());
}

/*!
    Sets the prototype of the object to \a prototype, an object or null. A prototype that would make a
    cycle is ignored.
*/
void QJSValue::setPrototype(const QJSValue &prototype) {
    if (!isObject())
        return;
    const Value p = EngineAccess::value(d->rt, prototype);
    if (p.isNull()) {
        d->value.asObject()->proto = nullptr;
        return;
    }
    if (!p.isObject())
        return;
    for (Object *o = p.asObject(); o; o = o->proto)
        if (o == d->value.asObject())
            return;
    d->value.asObject()->proto = p.asObject();
}

/*!
    Returns the property \a name of the object, or undefined. A getter that throws gives the error.
*/
QJSValue QJSValue::property(const QString &name) const {
    if (!isObject())
        return QJSValue();
    const Value self = d->value;
    return run(d->rt, [&] { return d->rt->get(self, name); });
}

/*!
    Sets the property \a name of the object to \a value.
*/
void QJSValue::setProperty(const QString &name, const QJSValue &value) {
    if (!isObject())
        return;
    Runtime *rt = d->rt;
    try {
        rt->put(d->value, name, EngineAccess::value(rt, value));
    } catch (const QtJSPrivate::Throw &) {
        return; // a setter that throws, or a read-only property: the value is not set
    }
}

/*!
    Returns \c true if the object or its prototypes have the property \a name; otherwise returns \c false.
*/
bool QJSValue::hasProperty(const QString &name) const {
    if (!isObject())
        return false;
    try {
        return d->rt->has(d->value.asObject(), name);
    } catch (const QtJSPrivate::Throw &) {
        return false;
    }
}

/*!
    Returns \c true if the object itself has the property \a name; otherwise returns \c false.
*/
bool QJSValue::hasOwnProperty(const QString &name) const {
    if (!isObject())
        return false;
    try {
        return d->value.asObject()->hasOwn(*d->rt, name);
    } catch (const QtJSPrivate::Throw &) {
        return false;
    }
}

/*!
    \overload

    Returns the element \a arrayIndex of the object.
*/
QJSValue QJSValue::property(quint32 arrayIndex) const { return property(QString::number(arrayIndex)); }

/*!
    \overload

    Sets the element \a arrayIndex of the object to \a value.
*/
void QJSValue::setProperty(quint32 arrayIndex, const QJSValue &value) {
    setProperty(QString::number(arrayIndex), value);
}

/*!
    Deletes the property \a name of the object. Returns \c true on success.
*/
bool QJSValue::deleteProperty(const QString &name) {
    if (!isObject())
        return false;
    try {
        return d->value.asObject()->deleteOwn(*d->rt, name);
    } catch (const QtJSPrivate::Throw &) {
        return false;
    }
}

/*!
    Calls the function with \a args and \c this undefined, and returns its result, or the error it throws.
*/
QJSValue QJSValue::call(const QJSValueList &args) const { return callWithInstance(QJSValue(), args); }

/*!
    Calls the function with \a args and \a instance as \c this, and returns its result, or the error it
    throws.
*/
QJSValue QJSValue::callWithInstance(const QJSValue &instance, const QJSValueList &args) const {
    if (!isCallable())
        return QJSValue();
    Runtime *rt = d->rt;
    QtJSPrivate::Values values;
    for (const QJSValue &a : args)
        values.push_back(EngineAccess::value(rt, a));
    const Value self = EngineAccess::value(rt, instance);
    return run(rt, [&] { return rt->call(d->value, self, values); });
}

/*!
    Calls the function as a constructor, as \c new does, with \a args, and returns the new object, or the
    error it throws.
*/
QJSValue QJSValue::callAsConstructor(const QJSValueList &args) const {
    if (!isCallable())
        return QJSValue();
    Runtime *rt = d->rt;
    QtJSPrivate::Values values;
    for (const QJSValue &a : args)
        values.push_back(EngineAccess::value(rt, a));
    return run(rt, [&] { return rt->construct(d->value, values); });
}

/*!
    Returns the type of the error, or NoError if the value is not an error.
*/
QJSValue::ErrorType QJSValue::errorType() const {
    if (!isError())
        return NoError;
    Object *e = d->value.asObject();
    for (int i = 0; i < 7; ++i)
        for (Object *p = e->proto; p; p = p->proto)
            if (p == d->rt->errorProtos[i] && i > 0)
                return static_cast<ErrorType>(i + 1);
    return GenericError;
}

/*!
    \class QJSValueIterator
    \brief The QJSValueIterator class iterates over the enumerable own properties of an object.

    \code
    QJSValueIterator it(object);
    while (it.hasNext()) {
        it.next();
        qDebug() << it.name() << it.value().toString();
    }
    \endcode

    The names are read when the iterator is made; properties added later are not seen.

    \differences
    None.
    \enddifferences

    \sa QJSValue::property()
*/

/*!
    Constructs an iterator over the properties of \a object, before the first one.
*/
QJSValueIterator::QJSValueIterator(const QJSValue &object) : m_object(object) {
    if (!object.isObject())
        return;
    const QJSValuePrivate *d = EngineAccess::data(object);
    try {
        m_keys = d->value.asObject()->enumerableKeys(*d->rt);
    } catch (const QtJSPrivate::Throw &) {
        m_keys.clear();
    }
}

/*!
    Destroys the iterator.
*/
QJSValueIterator::~QJSValueIterator() = default;

/*!
    Returns \c true if there is a property after the current one; otherwise returns \c false.
*/
bool QJSValueIterator::hasNext() const { return m_index + 1 < m_keys.size(); }

/*!
    Moves to the next property. Returns \c true if there was one.
*/
bool QJSValueIterator::next() {
    if (!hasNext()) {
        m_index = m_keys.size();
        return false;
    }
    ++m_index;
    return true;
}

/*!
    Returns the name of the current property.
*/
QString QJSValueIterator::name() const { return m_keys.value(m_index); }

/*!
    Returns the value of the current property.
*/
QJSValue QJSValueIterator::value() const { return m_object.property(name()); }

/*!
    Starts iterating over \a object, and returns a reference to the iterator.
*/
QJSValueIterator &QJSValueIterator::operator=(QJSValue &object) {
    m_object = object;
    m_keys.clear();
    m_index = -1;
    if (object.isObject()) {
        const QJSValuePrivate *d = EngineAccess::data(object);
        try {
            m_keys = d->value.asObject()->enumerableKeys(*d->rt);
        } catch (const QtJSPrivate::Throw &) {
            m_keys.clear();
        }
    }
    return *this;
}
