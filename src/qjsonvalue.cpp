// MIT License - see LICENSE
#include <QtCore/qjsonvalue.h>

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qvariant.h>

#include "qjson_p.h"

#include <climits>
#include <cmath>
#include <limits>
#include <cstring>

namespace {
constexpr qint64 kMaxExactInteger = qint64(1) << 53;

bool isWholeNumber(double d, qint64 *out) {
    if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0) || d != std::floor(d))
        return false;
    *out = static_cast<qint64>(d);
    return true;
}
} // namespace

const QString &QJsonPrivateAccess::string(const QJsonValue &v) {
    static const QString empty = QString::fromUtf8("");
    return v.m_data ? *static_cast<const QString *>(v.m_data.get()) : empty;
}

QJsonValue QJsonPrivateAccess::fromArrayData(std::shared_ptr<QJsonArrayPrivate> d) {
    QJsonValue v(QJsonValue::Array);
    v.m_data = std::move(d);
    return v;
}

QJsonValue QJsonPrivateAccess::fromObjectData(std::shared_ptr<QJsonObjectPrivate> d) {
    QJsonValue v(QJsonValue::Object);
    v.m_data = std::move(d);
    return v;
}

/*!
    \class QJsonValue
    \brief The QJsonValue class encapsulates a value in JSON.

    A value in JSON can be one of six basic types: a bool, a number, a string,
    an array, an object, or null. QJsonValue holds one of them, together with
    a seventh state, \c Undefined, which is what a lookup that finds nothing
    returns.

    \code
    QJsonObject o = doc.object();
    QJsonValue v = o.value("count");
    if (v.isDouble())
        use(v.toInt());
    \endcode

    Numbers are stored as either an exact integer or a double. Integers whose
    magnitude does not exceed 2^53 stay exact and are written without a
    decimal point; larger values and fractions are doubles. NaN and infinity
    cannot be represented in JSON, so they become null.

    The accessor functions such as toString() and toInt() return a default
    value when the value has another type, and never convert between types.
    Values are copied cheaply: strings, arrays and objects are shared until
    they are modified.

    \differences
    There are no constructors that take a QVariant, as in Qt 6; use
    fromVariant(). Overloads taking a C string key are provided for the \c{operator[]}
    functions of QJsonValue, QJsonObject and QJsonDocument, because
    QLatin1StringView converts implicitly in Nt, which would make a literal
    key ambiguous. A reference (QJsonValueRef) to an element that no longer
    exists reads as undefined instead of asserting.
    \enddifferences

    \sa QJsonObject, QJsonArray, QJsonDocument
*/

/*!
    \enum QJsonValue::Type

    The type of a QJsonValue.

    \value Null A null value
    \value Bool A boolean
    \value Double A number, either an integer or a double
    \value String A string
    \value Array An array
    \value Object An object
    \value Undefined The value of a lookup that found nothing
*/

/*!
    Creates a QJsonValue of the given \a type, holding its default: \c false,
    \c 0, an empty string, an empty array or an empty object.
*/
QJsonValue::QJsonValue(Type type) : m_type(type) {
    switch (type) {
    case Bool:
        m_bool = false;
        break;
    case Double:
        m_isInt = true;
        m_int = 0;
        break;
    case String:
        m_data = std::make_shared<QString>(QString::fromUtf8(""));
        break;
    case Null:
    case Array:
    case Object:
    case Undefined:
        break;
    }
}

/*!
    Creates a value of type \c Bool holding \a b.
*/
QJsonValue::QJsonValue(bool b) : m_type(Bool) { m_bool = b; }

/*!
    Creates a value of type \c Double holding \a n. NaN and infinity cannot
    be stored in JSON, so they give a null value.
*/
QJsonValue::QJsonValue(double n) {
    if (std::isfinite(n)) {
        m_type = Double;
        m_double = n;
    }
}

/*!
    \overload

    Creates a value of type \c Double holding the integer \a n.
*/
QJsonValue::QJsonValue(int n) : QJsonValue(static_cast<qint64>(n)) {}

/*!
    \overload

    Creates a value of type \c Double holding the integer \a v. Values up to
    2^53 in magnitude are kept exact; larger ones are rounded to the
    nearest double.
*/
QJsonValue::QJsonValue(qint64 v) : m_type(Double) {
    if (v >= -kMaxExactInteger && v <= kMaxExactInteger) {
        m_isInt = true;
        m_int = v;
    } else {
        m_double = static_cast<double>(v);
    }
}

/*!
    Creates a value of type \c String holding \a s.
*/
QJsonValue::QJsonValue(const QString &s) : m_type(String), m_data(std::make_shared<QString>(s)) {}

/*!
    \overload

    Creates a value of type \c String holding the Latin-1 text \a s.
*/
QJsonValue::QJsonValue(QLatin1StringView s) : QJsonValue(QString(s)) {}

/*!
    \overload

    Creates a value of type \c String holding the UTF-8 text \a s.
*/
QJsonValue::QJsonValue(const char *s) : QJsonValue(QString::fromUtf8(s)) {}

/*!
    Creates a value of type \c Array holding \a a.
*/
QJsonValue::QJsonValue(const QJsonArray &a) : m_type(Array), m_data(QJsonPrivateAccess::data(a)) {}

/*!
    Creates a value of type \c Object holding \a o.
*/
QJsonValue::QJsonValue(const QJsonObject &o) : m_type(Object), m_data(QJsonPrivateAccess::data(o)) {}

/*!
    Creates a copy of the value that \a ref refers to.
*/
QJsonValue::QJsonValue(const QJsonValueConstRef &ref) : QJsonValue(ref.concrete()) {}

/*!
    \fn QJsonValue::QJsonValue(const QJsonValue &other)

    Creates a copy of \a other.
*/

/*!
    Moves \a other into the new value, which leaves \a other null.
*/
QJsonValue::QJsonValue(QJsonValue &&other) noexcept
    : m_type(other.m_type), m_isInt(other.m_isInt), m_data(std::move(other.m_data)) {
    std::memcpy(&m_int, &other.m_int, sizeof m_int);
    other.m_type = Null;
}

/*!
    \fn QJsonValue::~QJsonValue()

    Destroys the value.
*/

/*!
    \fn QJsonValue &QJsonValue::operator=(const QJsonValue &other)

    Assigns \a other to this value and returns a reference to it.
*/

/*!
    Moves \a other into this value, which leaves \a other null, and returns a
    reference to this value.
*/
QJsonValue &QJsonValue::operator=(QJsonValue &&other) noexcept {
    if (this != &other) {
        m_type = other.m_type;
        m_isInt = other.m_isInt;
        std::memcpy(&m_int, &other.m_int, sizeof m_int);
        m_data = std::move(other.m_data);
        other.m_type = Null;
    }
    return *this;
}

/*!
    \fn void QJsonValue::swap(QJsonValue &other)

    Swaps this value with \a other.
*/

/*!
    \fn QJsonValue::Type QJsonValue::type() const

    Returns the type of the value.

    \sa isNull(), isBool(), isDouble(), isString(), isArray(), isObject(), isUndefined()
*/

/*!
    \fn bool QJsonValue::isNull() const

    Returns \c true if the value is null.
*/

/*!
    \fn bool QJsonValue::isBool() const

    Returns \c true if the value is a bool.
*/

/*!
    \fn bool QJsonValue::isDouble() const

    Returns \c true if the value is a number; integers count as well.
*/

/*!
    \fn bool QJsonValue::isString() const

    Returns \c true if the value is a string.
*/

/*!
    \fn bool QJsonValue::isArray() const

    Returns \c true if the value is an array.
*/

/*!
    \fn bool QJsonValue::isObject() const

    Returns \c true if the value is an object.
*/

/*!
    \fn bool QJsonValue::isUndefined() const

    Returns \c true if the value is undefined. A lookup of a missing key or
    index gives an undefined value.
*/

/*!
    Returns the bool the value holds, or \a defaultValue if the value is not
    a bool.
*/
bool QJsonValue::toBool(bool defaultValue) const { return m_type == Bool ? m_bool : defaultValue; }

/*!
    Returns the value as an \c int, or \a defaultValue if the value is not a
    number, is not a whole number or does not fit in an \c int.

    \sa toInteger(), toDouble()
*/
int QJsonValue::toInt(int defaultValue) const {
    const qint64 n = toInteger(defaultValue);
    return n >= INT_MIN && n <= INT_MAX ? static_cast<int>(n) : defaultValue;
}

/*!
    Returns the value as a \c qint64, or \a defaultValue if the value is not
    a number, is not a whole number or does not fit in a \c qint64.

    \sa toInt(), toDouble()
*/
qint64 QJsonValue::toInteger(qint64 defaultValue) const {
    if (m_type != Double)
        return defaultValue;
    if (m_isInt)
        return m_int;
    qint64 n = 0;
    return isWholeNumber(m_double, &n) ? n : defaultValue;
}

/*!
    Returns the value as a \c double, or \a defaultValue if the value is not a
    number.

    \sa toInt(), toInteger()
*/
double QJsonValue::toDouble(double defaultValue) const {
    return m_type == Double ? QJsonPrivateAccess::number(*this) : defaultValue;
}

/*!
    Returns the string the value holds, or a null string if the value is not
    a string.
*/
QString QJsonValue::toString() const { return m_type == String ? QJsonPrivateAccess::string(*this) : QString(); }

/*!
    \overload

    Returns the string the value holds, or \a defaultValue if the value is
    not a string.
*/
QString QJsonValue::toString(const QString &defaultValue) const {
    return m_type == String ? QJsonPrivateAccess::string(*this) : defaultValue;
}

/*!
    \since 6.10
    Returns a view of the string of the value, or \a defaultValue if the value is not a string. The view stays
    valid as long as the string is not changed or destroyed, in this value or in a copy that shares it.

    \sa toString()
*/
QAnyStringView QJsonValue::toStringView(QAnyStringView defaultValue) const {
    return m_type == String ? QAnyStringView(QJsonPrivateAccess::string(*this)) : defaultValue;
}

/*!
    \since 6.9
    Parses \a json, the text of one JSON value of any type (not only an array or an object), and returns the
    value. On failure, returns a null value and, if \a error is not null, sets it to what went wrong and where.

    \code
    QJsonValue::fromJson("[1, 2]").toArray().size(); // 2
    QJsonValue::fromJson("\"text\"").toString();     // "text"
    \endcode

    \sa toJson(), QJsonDocument::fromJson()
*/
QJsonValue QJsonValue::fromJson(QByteArrayView json, QJsonParseError *error) {
    return QJsonPrivate::parse(json.toByteArray(), error, true);
}

/*!
    \since 6.9
    Returns the value as JSON text in UTF-8, laid out as \a format says. An undefined value gives an empty
    byte array.

    \sa fromJson(), QJsonDocument::toJson()
*/
QByteArray QJsonValue::toJson(JsonFormat format) const {
    if (m_type == Undefined)
        return QByteArray();
    const std::string text = QJsonPrivate::write(*this, format == JsonFormat::Compact);
    QByteArray out(text.data(), static_cast<qsizetype>(text.size()));
    if (format == JsonFormat::Indented && (m_type == Array || m_type == Object))
        out.append('\n');
    return out;
}

/*!
    Returns the array the value holds, or an empty array if the value is not
    an array.
*/
QJsonArray QJsonValue::toArray() const { return toArray(QJsonArray()); }

/*!
    \overload

    Returns the array the value holds, or \a defaultValue if the value is not
    an array.
*/
QJsonArray QJsonValue::toArray(const QJsonArray &defaultValue) const {
    if (m_type != Array)
        return defaultValue;
    return QJsonPrivateAccess::arrayFromData(std::static_pointer_cast<QJsonArrayPrivate>(m_data));
}

/*!
    Returns the object the value holds, or an empty object if the value is
    not an object.
*/
QJsonObject QJsonValue::toObject() const { return toObject(QJsonObject()); }

/*!
    \overload

    Returns the object the value holds, or \a defaultValue if the value is
    not an object.
*/
QJsonObject QJsonValue::toObject(const QJsonObject &defaultValue) const {
    if (m_type != Object)
        return defaultValue;
    return QJsonPrivateAccess::objectFromData(std::static_pointer_cast<QJsonObjectPrivate>(m_data));
}

/*!
    Converts \a variant to a QJsonValue.

    \table
    \header \li QVariant type \li QJsonValue type
    \row \li an invalid variant, \c std::nullptr_t \li Null
    \row \li \c bool \li Bool
    \row \li \c short, \c{unsigned short}, \c int, \c{unsigned int}, \c long, \c qlonglong
              \li Double, kept as an exact integer up to 2^53
    \row \li \c qulonglong \li Double; exact up to 2^53, rounded above that
    \row \li \c float, \c double \li Double; NaN and infinity give Null
    \row \li QString \li String
    \row \li QStringList, QVariantList \li Array
    \row \li QVariantMap, QVariantHash \li Object
    \row \li QUrl \li String, the fully encoded form
    \row \li QUuid \li String, without braces
    \row \li QJsonValue, QJsonArray, QJsonObject \li the same value
    \row \li QJsonDocument \li Array or Object, an empty Object for a null document
    \row \li any other type \li String if it converts to a non-empty QString, as
              QByteArray, QChar, QDate, QTime and QDateTime do (the dates and times in ISO 8601
              form); otherwise Null
    \endtable

    \sa toVariant(), QJsonArray::fromVariantList(), QJsonObject::fromVariantMap()
*/
QJsonValue QJsonValue::fromVariant(const QVariant &variant) {
    switch (variant.typeId()) {
    case QMetaType::Nullptr:
        return QJsonValue(Null);
    case QMetaType::Bool:
        return QJsonValue(variant.toBool());
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
        return QJsonValue(static_cast<qint64>(variant.toLongLong()));
    case QMetaType::ULong:
    case QMetaType::ULongLong: {
        const qulonglong n = variant.toULongLong();
        if (n <= static_cast<qulonglong>(std::numeric_limits<qint64>::max()))
            return QJsonValue(static_cast<qint64>(n));
        return QJsonValue(static_cast<double>(n));
    }
    case QMetaType::Float:
    case QMetaType::Double:
        return QJsonValue(variant.toDouble());
    case QMetaType::QString:
        return QJsonValue(variant.toString());
    case QMetaType::QStringList:
        return QJsonValue(QJsonArray::fromStringList(variant.toStringList()));
    case QMetaType::QVariantList:
        return QJsonValue(QJsonArray::fromVariantList(variant.toList()));
    case QMetaType::QVariantMap:
        return QJsonValue(QJsonObject::fromVariantMap(variant.toMap()));
    case QMetaType::QVariantHash:
        return QJsonValue(QJsonObject::fromVariantHash(variant.toHash()));
    case QMetaType::QUrl:
        return QJsonValue(QString::fromLatin1(variant.toUrl().toEncoded()));
    case QMetaType::QUuid:
        return QJsonValue(variant.value<QUuid>().toString(QUuid::WithoutBraces));
    case QMetaType::QJsonValue:
        return variant.value<QJsonValue>();
    case QMetaType::QJsonArray:
        return QJsonValue(variant.value<QJsonArray>());
    case QMetaType::QJsonObject:
        return QJsonValue(variant.value<QJsonObject>());
    case QMetaType::QJsonDocument: {
        const QJsonDocument doc = variant.value<QJsonDocument>();
        return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    }
    default:
        break;
    }
    const QString string = variant.toString();
    return string.isEmpty() ? QJsonValue(Null) : QJsonValue(string);
}

/*!
    Converts the value to a QVariant.

    \table
    \header \li QJsonValue type \li QVariant type
    \row \li Null \li \c std::nullptr_t
    \row \li Bool \li \c bool
    \row \li Double \li \c qlonglong if the value was stored as an integer, otherwise
              \c double
    \row \li String \li QString
    \row \li Array \li QVariantList
    \row \li Object \li QVariantMap
    \row \li Undefined \li an invalid QVariant
    \endtable

    \sa fromVariant(), QJsonArray::toVariantList(), QJsonObject::toVariantMap()
*/
QVariant QJsonValue::toVariant() const {
    switch (m_type) {
    case Null:
        return QVariant::fromValue(nullptr);
    case Bool:
        return QVariant(m_bool);
    case Double:
        if (m_isInt)
            return QVariant(static_cast<qlonglong>(m_int));
        return QVariant(m_double);
    case String:
        return QVariant(QJsonPrivateAccess::string(*this));
    case Array:
        return QVariant(toArray().toVariantList());
    case Object:
        return QVariant(toObject().toVariantMap());
    case Undefined:
        break;
    }
    return QVariant();
}

/*!
    Returns the value that \a key maps to if this value is an object, and an
    undefined value if it is not an object or has no such key.

    \sa QJsonObject::value()
*/
QJsonValue QJsonValue::operator[](const QString &key) const {
    return m_type == Object ? toObject().value(key) : QJsonValue(Undefined);
}

/*!
    \overload
*/
QJsonValue QJsonValue::operator[](QStringView key) const {
    return m_type == Object ? toObject().value(key) : QJsonValue(Undefined);
}

/*!
    \overload
*/
QJsonValue QJsonValue::operator[](QLatin1StringView key) const {
    return m_type == Object ? toObject().value(key) : QJsonValue(Undefined);
}

/*!
    \fn template <typename K, typename> QJsonValue QJsonValue::operator[](K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \overload

    Returns the element at index \a i if this value is an array, and an
    undefined value if it is not an array or \a i is out of range.

    \sa QJsonArray::at()
*/
QJsonValue QJsonValue::operator[](qsizetype i) const {
    return m_type == Array ? toArray().at(i) : QJsonValue(Undefined);
}

bool QJsonValue::equals(const QJsonValue &other) const {
    if (m_type != other.m_type)
        return false;
    switch (m_type) {
    case Null:
    case Undefined:
        return true;
    case Bool:
        return m_bool == other.m_bool;
    case Double:
        if (m_isInt && other.m_isInt)
            return m_int == other.m_int;
        return QJsonPrivateAccess::number(*this) == QJsonPrivateAccess::number(other);
    case String:
        return QJsonPrivateAccess::string(*this) == QJsonPrivateAccess::string(other);
    case Array:
        return toArray() == other.toArray();
    case Object:
        return toObject() == other.toObject();
    }
    return false;
}

/*!
    \class QJsonValueConstRef
    \brief The QJsonValueConstRef class is a read-only reference to a value inside an array or object.

    Its instances are what the const iterators of QJsonArray and QJsonObject
    return. It reads the element each time it is used, so it behaves like a
    \c{const QJsonValue &} and converts to QJsonValue when copied into one.

    \differences
    There is no conversion to \c QVariant. A reference does not keep the
    container alive, and reading through one after the container changed
    or was destroyed is not allowed.
    \enddifferences

    \sa QJsonValueRef, QJsonValue
*/

/*!
    \fn QJsonValueConstRef::QJsonValueConstRef(const QJsonValueConstRef &)

    Creates a copy of the reference; both refer to the same element.
*/

QJsonValue QJsonValueConstRef::concrete() const {
    if (m_array)
        return m_array->at(m_index);
    const auto &d = QJsonPrivateAccess::data(*m_object);
    if (!d || m_index < 0 || m_index >= static_cast<qsizetype>(d->items.size()))
        return QJsonValue(QJsonValue::Undefined);
    return d->items[static_cast<std::size_t>(m_index)].value;
}

/*!
    Returns the type of the element.
*/
QJsonValue::Type QJsonValueConstRef::type() const { return concrete().type(); }

/*!
    \fn bool QJsonValueConstRef::isNull() const

    Returns \c true if the element is null.
*/

/*!
    \fn bool QJsonValueConstRef::isBool() const

    Returns \c true if the element is a bool.
*/

/*!
    \fn bool QJsonValueConstRef::isDouble() const

    Returns \c true if the element is a number.
*/

/*!
    \fn bool QJsonValueConstRef::isString() const

    Returns \c true if the element is a string.
*/

/*!
    \fn bool QJsonValueConstRef::isArray() const

    Returns \c true if the element is an array.
*/

/*!
    \fn bool QJsonValueConstRef::isObject() const

    Returns \c true if the element is an object.
*/

/*!
    \fn bool QJsonValueConstRef::isUndefined() const

    Returns \c true if the element is undefined, which it is when the
    reference is out of range.
*/

/*!
    Returns the element as a bool, or \a defaultValue if it is not one.

    \sa QJsonValue::toBool()
*/
bool QJsonValueConstRef::toBool(bool defaultValue) const { return concrete().toBool(defaultValue); }

/*!
    Returns the element as an \c int, or \a defaultValue if it is not a whole
    number that fits.

    \sa QJsonValue::toInt()
*/
int QJsonValueConstRef::toInt(int defaultValue) const { return concrete().toInt(defaultValue); }

/*!
    Returns the element as a \c qint64, or \a defaultValue if it is not a
    whole number that fits.

    \sa QJsonValue::toInteger()
*/
qint64 QJsonValueConstRef::toInteger(qint64 defaultValue) const { return concrete().toInteger(defaultValue); }

/*!
    Returns the element as a \c double, or \a defaultValue if it is not a
    number.

    \sa QJsonValue::toDouble()
*/
double QJsonValueConstRef::toDouble(double defaultValue) const { return concrete().toDouble(defaultValue); }

/*!
    Returns the element as a string, or a null string if it is not one.

    \sa QJsonValue::toString()
*/
QString QJsonValueConstRef::toString() const { return concrete().toString(); }

/*!
    \overload

    Returns the element as a string, or \a defaultValue if it is not one.
*/
QString QJsonValueConstRef::toString(const QString &defaultValue) const { return concrete().toString(defaultValue); }

/*!
    \since 6.10
    Returns a view of the string of the element, or \a defaultValue if it is not a string. The view stays valid as
    long as the element is not changed or removed.
*/
QAnyStringView QJsonValueConstRef::toStringView(QAnyStringView defaultValue) const {
    // The copy shares the string of the element, so the view points into the element.
    return concrete().toStringView(defaultValue);
}

/*!
    Returns the element as a QVariant.

    \sa QJsonValue::toVariant()
*/
QVariant QJsonValueConstRef::toVariant() const { return concrete().toVariant(); }

/*!
    Returns the element as an array, or an empty array if it is not one.

    \sa QJsonValue::toArray()
*/
QJsonArray QJsonValueConstRef::toArray() const { return concrete().toArray(); }

/*!
    \overload

    Returns the element as an array, or \a defaultValue if it is not one.
*/
QJsonArray QJsonValueConstRef::toArray(const QJsonArray &defaultValue) const {
    return concrete().toArray(defaultValue);
}

/*!
    Returns the element as an object, or an empty object if it is not one.

    \sa QJsonValue::toObject()
*/
QJsonObject QJsonValueConstRef::toObject() const { return concrete().toObject(); }

/*!
    \overload

    Returns the element as an object, or \a defaultValue if it is not one.
*/
QJsonObject QJsonValueConstRef::toObject(const QJsonObject &defaultValue) const {
    return concrete().toObject(defaultValue);
}

/*!
    Returns the value that \a key maps to if the element is an object, and
    an undefined value otherwise.
*/
QJsonValue QJsonValueConstRef::operator[](const QString &key) const { return concrete()[key]; }

/*!
    \overload
*/
QJsonValue QJsonValueConstRef::operator[](QStringView key) const { return concrete()[key]; }

/*!
    \overload
*/
QJsonValue QJsonValueConstRef::operator[](QLatin1StringView key) const { return concrete()[key]; }

/*!
    \fn template <typename K, typename> QJsonValue QJsonValueConstRef::operator[](K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \overload

    Returns the item at index \a i if the element is an array, and an
    undefined value otherwise.
*/
QJsonValue QJsonValueConstRef::operator[](qsizetype i) const { return concrete()[i]; }

/*!
    \class QJsonRefArrow
    \internal
*/

/*!
    \class QJsonValueRef
    \brief The QJsonValueRef class is a helper class for QJsonArray and QJsonObject.

    When you get an object of type QJsonValueRef, for example from
    the array's \c{operator[]} or by dereferencing a non-const iterator, you can
    use it as if it were a reference to a QJsonValue. Assigning to it replaces
    the element in the container it came from.

    \code
    QJsonArray array = {1, 2, 3};
    array[1] = "two";
    for (QJsonValueRef v : array) {
        if (v.isDouble())
            v = v.toInt() * 2;
    }
    \endcode

    \differences
    A reference stores the container and an index, so it reads whatever is at
    that index now. Changing the container's size while holding a reference
    can make it refer to another element or to none, which then reads as
    undefined; assigning through a reference to a missing element does
    nothing. Qt asserts in these cases.
    \enddifferences

    \sa QJsonValueConstRef, QJsonValue
*/

/*!
    \fn QJsonValueRef::QJsonValueRef(QJsonArray *array, qsizetype idx)

    Creates a reference to the element at index \a idx of \a array.
*/

/*!
    \fn QJsonValueRef::QJsonValueRef(QJsonObject *object, qsizetype idx)

    Creates a reference to the entry at position \a idx of \a object, in the
    sorted order of its keys.
*/

/*!
    \fn QJsonValueRef::QJsonValueRef(const QJsonValueRef &)

    Creates a copy of the reference; both refer to the same element.
*/

/*!
    Replaces the element with \a val and returns a reference to it. Nothing
    happens if the element does not exist.
*/
QJsonValueRef &QJsonValueRef::operator=(const QJsonValue &val) {
    if (m_array)
        const_cast<QJsonArray *>(m_array)->replace(m_index, val);
    else
        QJsonPrivateAccess::setObjectValue(*const_cast<QJsonObject *>(m_object), m_index, val);
    return *this;
}

/*!
    \overload

    Replaces the element with the value \a val refers to. This assigns the
    value, not the reference.
*/
QJsonValueRef &QJsonValueRef::operator=(const QJsonValueRef &val) { return *this = QJsonValue(val); }
