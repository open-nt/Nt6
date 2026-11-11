// MIT License - see LICENSE
#include <QtCore/qcborvalue.h>

#include <QtCore/qcborarray.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qcborstreamreader.h>
#include <QtCore/qcborstreamwriter.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qvariant.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

/*!
    \class QCborValue
    \brief The QCborValue class encapsulates a value in CBOR.
    \since 5.12

    A QCborValue holds one value of CBOR (RFC 8949): an integer, a floating-point number, a
    string, a byte array, \c true, \c false, \c null or \c undefined, an array (QCborArray), a map
    (QCborMap), or a tag and the value it applies to. Some tags are known and turned into Nt types:
    a date and time, a URL, a regular expression and a UUID.

    \code
    QCborMap map;
    map[u"name"_s] = u"Nt"_s;
    map[u"sizes"_s] = QCborArray{1, 2.5, true};
    const QByteArray cbor = QCborValue(map).toCbor();

    QCborParserError error;
    const QCborValue back = QCborValue::fromCbor(cbor, &error);
    if (error.error == QCborError::NoError)
        qDebug() << back[u"name"_s].toString() << back.toDiagnosticNotation();
    \endcode

    Values convert to and from QVariant and QJsonValue, and show themselves in the diagnostic
    notation of CBOR with toDiagnosticNotation().

    A QCborValue shares its string, byte array or container with its copies until one of them is
    changed. The operator[] that is not const returns a QCborValueRef, through which a value in a
    container is changed in place.

    \differences
    An unsigned integer above the largest \c qint64, and a negative one below the smallest, become
    a Double, as in Qt. A date and time is read only from the tag QCborKnownTags::DateTimeString,
    not from QCborKnownTags::UnixTime_t. An invalid value is written as \c undefined by toCbor(),
    so that the containers around it stay valid. A value that could not be read whole by
    fromCbor() is returned as far as it was read. There is no \c QDebug operator, and the
    functions that take a key take a \c{const QString &} and not a QAnyStringView.
    \enddifferences

    \sa QCborArray, QCborMap, QCborStreamReader, QCborStreamWriter
*/

/*!
    \class QCborValueConstRef
    \brief The QCborValueConstRef class is a reference to a value in a QCborArray or QCborMap that cannot be changed.
    \since 6.4

    The const iterators of the containers and the keys of a map give such a reference. It reads as
    a QCborValue does; converting it gives a copy.

    \differences
    The reference points to the value in the container: it is invalid once the container gets or
    loses elements.
    \enddifferences

    \sa QCborValueRef
*/

/*!
    \class QCborValueRef
    \brief The QCborValueRef class is a reference to a value in a QCborArray or QCborMap.
    \since 5.12

    The operator[] of QCborValue, QCborArray and QCborMap that is not const, and their iterators,
    give such a reference. Assigning to it changes the value in the container:

    \code
    QCborArray array{1, 2, 3};
    array[1] = u"two"_s;
    \endcode

    \differences
    The reference points to the value in the container: it is invalid once the container gets or
    loses elements.
    \enddifferences

    \sa QCborValueConstRef
*/

struct QCborValue::Payload {
    QByteArray bytes;  // ByteArray
    QString text;      // String
    QCborArray array;  // Array
    QCborMap map;      // Map
    QCborValue tagged; // Tag and the extended types: the value that the tag applies to
};

namespace {

constexpr int kMaxDepth = 1024;

// The shortest text that reads back as the same double.
QString shortestDouble(double v) {
    char buffer[40];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof buffer, "%.*g", precision, v);
        if (std::strtod(buffer, nullptr) == v)
            break;
    }
    // A whole number below 10^16 is written out, as the examples of the RFC are.
    if (std::trunc(v) == v && std::fabs(v) < 1e16)
        std::snprintf(buffer, sizeof buffer, "%.0f", v);
    QString text = QString::fromLatin1(buffer);
    if (!text.contains(QChar(u'.')) && !text.contains(QChar(u'e')) && !text.contains(QChar(u'n')))
        text += QString(".0");
    return text;
}

QString quoted(const QString &s) {
    QString out("\"");
    for (const QChar c : s) {
        const char16_t u = c.unicode();
        switch (u) {
        case u'"':
            out += QString("\\\"");
            break;
        case u'\\':
            out += QString("\\\\");
            break;
        case u'\n':
            out += QString("\\n");
            break;
        case u'\r':
            out += QString("\\r");
            break;
        case u'\t':
            out += QString("\\t");
            break;
        case u'\b':
            out += QString("\\b");
            break;
        case u'\f':
            out += QString("\\f");
            break;
        default:
            if (u < 0x20 || u == 0x7f) {
                char escape[8];
                std::snprintf(escape, sizeof escape, "\\u%04x", static_cast<unsigned>(u));
                out += QString::fromLatin1(escape);
            } else {
                out += c;
            }
        }
    }
    out += QChar(u'"');
    return out;
}

// The order of compare() within one type of container or string: shorter first, then element by element.
template <typename T> int sign(T a, T b) { return a < b ? -1 : (b < a ? 1 : 0); }

int compareBytes(const QByteArray &a, const QByteArray &b) {
    if (a.size() != b.size())
        return a.size() < b.size() ? -1 : 1;
    const int c = std::memcmp(a.constData(), b.constData(), static_cast<std::size_t>(a.size()));
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

} // namespace

/*!
    Constructs an \c undefined value.
*/
QCborValue::QCborValue() noexcept = default;

/*!
    Constructs a value of the type \a t_ with the default value of that type: 0, an empty string
    or container, \c false, the simple type 0. For a tag or an extended type the tag is 0 and the
    tagged value is \c undefined.
*/
QCborValue::QCborValue(Type t_) : t(t_) {
    if (t_ == Tag || t_ >= DateTime) {
        t = Tag;
        payload();
    } else if (t_ == ByteArray || t_ == String || t_ == Array || t_ == Map) {
        payload();
    }
}

/*!
    Constructs a \c null value.
*/
QCborValue::QCborValue(std::nullptr_t) : t(Null) {}

/*!
    Constructs \c true or \c false, according to \a b.
*/
QCborValue::QCborValue(bool b) : t(b ? True : False) {}

/*!
    Constructs an integer from \a i.
*/
QCborValue::QCborValue(int i) : t(Integer), n(i) {}

/*!
    Constructs an integer from \a u.
*/
QCborValue::QCborValue(unsigned u) : t(Integer), n(u) {}

/*!
    Constructs an integer from \a i.
*/
QCborValue::QCborValue(qint64 i) : t(Integer), n(i) {}

/*!
    Constructs a floating-point number from \a v.
*/
QCborValue::QCborValue(double v) : t(Double), dbl(v) {}

/*!
    Constructs the simple type \a st. The four with a meaning give \c false, \c true, \c null
    and \c undefined.
*/
QCborValue::QCborValue(QCborSimpleType st) : t(static_cast<Type>(SimpleType + int(st))) {}

/*!
    Constructs a byte array from \a ba.
*/
QCborValue::QCborValue(const QByteArray &ba) : t(ByteArray) { payload().bytes = ba; }

/*!
    Constructs a string from \a s.
*/
QCborValue::QCborValue(const QString &s) : t(String) { payload().text = s; }

/*!
    \overload
    Constructs a string from \a s.
*/
QCborValue::QCborValue(QStringView s) : QCborValue(s.toString()) {}

/*!
    \overload
    Constructs a string from the Latin-1 text \a s.
*/
QCborValue::QCborValue(QLatin1StringView s) : QCborValue(QString(s)) {}

/*!
    \overload
    Constructs a string from the UTF-8 text \a s.
*/
QCborValue::QCborValue(const char *s) : QCborValue(QString::fromUtf8(s)) {}

/*!
    Constructs an array from \a a.
*/
QCborValue::QCborValue(const QCborArray &a) : t(Array) { payload().array = a; }

/*!
    Constructs a map from \a m.
*/
QCborValue::QCborValue(const QCborMap &m) : t(Map) { payload().map = m; }

/*!
    Constructs the tag \a tag, which applies to \a taggedValue. A known tag whose value is of the
    right kind gives one of the extended types: QCborKnownTags::DateTimeString with a string that
    is a date and time gives a DateTime.

    \sa tag(), taggedValue()
*/
QCborValue::QCborValue(QCborTag tag, const QCborValue &taggedValue) : t(Tag), n(static_cast<qint64>(tag)) {
    payload().tagged = taggedValue;
    // The tags that QCborValue knows become their extended types when what they apply to fits.
    switch (static_cast<quint64>(tag)) {
    case quint64(QCborKnownTags::DateTimeString):
        if (taggedValue.isString() && QDateTime::fromString(taggedValue.toString(), Qt::ISODateWithMs).isValid())
            t = DateTime;
        break;
    case quint64(QCborKnownTags::Url):
        if (taggedValue.isString())
            t = Url;
        break;
    case quint64(QCborKnownTags::RegularExpression):
        if (taggedValue.isString())
            t = RegularExpression;
        break;
    case quint64(QCborKnownTags::Uuid):
        if (taggedValue.isByteArray() && taggedValue.toByteArray().size() == 16)
            t = Uuid;
        break;
    default:
        break;
    }
}

/*!
    \overload
    Constructs the known tag \a tag, which applies to \a taggedValue.
*/
QCborValue::QCborValue(QCborKnownTags tag, const QCborValue &taggedValue)
    : QCborValue(QCborTag(static_cast<quint64>(tag)), taggedValue) {}

/*!
    Constructs a date and time from \a dt, as the tag QCborKnownTags::DateTimeString with the
    date and time in ISO 8601 with milliseconds.
*/
QCborValue::QCborValue(const QDateTime &dt)
    : QCborValue(QCborKnownTags::DateTimeString, QCborValue(dt.toString(Qt::ISODateWithMs))) {
    t = DateTime;
}

/*!
    Constructs a URL from \a url, as the tag QCborKnownTags::Url with the URL as text.
*/
QCborValue::QCborValue(const QUrl &url) : QCborValue(QCborKnownTags::Url, QCborValue(url.toString())) {}

/*!
    Constructs a regular expression from \a rx, as the tag QCborKnownTags::RegularExpression with
    its pattern. The options of the expression are not kept.
*/
QCborValue::QCborValue(const QRegularExpression &rx)
    : QCborValue(QCborKnownTags::RegularExpression, QCborValue(rx.pattern())) {}

/*!
    Constructs a UUID from \a uuid, as the tag QCborKnownTags::Uuid with its 16 bytes.
*/
QCborValue::QCborValue(const QUuid &uuid) : QCborValue(QCborKnownTags::Uuid, QCborValue(uuid.toRfc4122())) {}

/*!
    Destroys the value.
*/
QCborValue::~QCborValue() = default;

/*!
    Constructs a copy of \a other. The copy shares the string or container of \a other until one
    of them is changed.
*/
QCborValue::QCborValue(const QCborValue &other) = default;

/*!
    Move-constructs a value from \a other, which becomes \c undefined.
*/
QCborValue::QCborValue(QCborValue &&other) noexcept
    : t(std::exchange(other.t, Undefined)), n(other.n), dbl(other.dbl), d(std::move(other.d)) {}

/*!
    Assigns \a other to this value and returns a reference to it.
*/
QCborValue &QCborValue::operator=(const QCborValue &other) = default;

/*!
    Move-assigns \a other to this value and returns a reference to it.
*/
QCborValue &QCborValue::operator=(QCborValue &&other) noexcept {
    QCborValue moved(std::move(other));
    swap(moved);
    return *this;
}

/*!
    Exchanges this value with \a other.
*/
void QCborValue::swap(QCborValue &other) noexcept {
    std::swap(t, other.t);
    std::swap(n, other.n);
    std::swap(dbl, other.dbl);
    std::swap(d, other.d);
}

// The payload, which this value no longer shares with a copy.
QCborValue::Payload &QCborValue::payload() {
    if (!d)
        d = std::make_shared<Payload>();
    else if (d.use_count() > 1)
        d = std::make_shared<Payload>(*d);
    return *d;
}

/*!
    Returns the simple type of the value, or \a defaultValue if it is not a simple type.
*/
QCborSimpleType QCborValue::toSimpleType(QCborSimpleType defaultValue) const {
    // Every value of the underlying type is a simple type, not only those with a name.
    return isSimpleType() ? static_cast<QCborSimpleType>(t - SimpleType) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
                          : defaultValue;
}

/*!
    Returns the integer of the value, or \a defaultValue if it is not an integer. A
    floating-point number is not converted.

    \sa toDouble()
*/
qint64 QCborValue::toInteger(qint64 defaultValue) const { return t == Integer ? n : defaultValue; }

/*!
    Returns \c true or \c false, or \a defaultValue if the value is not a boolean.
*/
bool QCborValue::toBool(bool defaultValue) const { return isBool() ? t == True : defaultValue; }

/*!
    Returns the floating-point number of the value, or the integer converted to one, or
    \a defaultValue if the value is no number.
*/
double QCborValue::toDouble(double defaultValue) const {
    if (t == Double)
        return dbl;
    if (t == Integer)
        return static_cast<double>(n);
    return defaultValue;
}

/*!
    Returns the tag of the value, also of an extended type, or \a defaultValue if it is not a
    tag.

    \sa taggedValue(), isTag()
*/
QCborTag QCborValue::tag(QCborTag defaultValue) const { return isTag() ? QCborTag(static_cast<quint64>(n)) : defaultValue; }

/*!
    Returns the value that the tag applies to, or \a defaultValue if this is not a tag. For an
    extended type this is the string or byte array of the encoding.

    \sa tag()
*/
QCborValue QCborValue::taggedValue(const QCborValue &defaultValue) const {
    return isTag() && d ? d->tagged : defaultValue;
}

/*!
    Returns the byte array of the value, or \a defaultValue if it is not a byte array.
*/
QByteArray QCborValue::toByteArray(const QByteArray &defaultValue) const {
    return t == ByteArray && d ? d->bytes : defaultValue;
}

/*!
    Returns the string of the value, or \a defaultValue if it is not a string.
*/
QString QCborValue::toString(const QString &defaultValue) const { return t == String && d ? d->text : defaultValue; }

/*!
    Returns the date and time of the value, or \a defaultValue if it is not a date and time.
*/
QDateTime QCborValue::toDateTime(const QDateTime &defaultValue) const {
    if (t != DateTime || !d)
        return defaultValue;
    return QDateTime::fromString(d->tagged.toString(), Qt::ISODateWithMs);
}

/*!
    Returns the URL of the value, or \a defaultValue if it is not a URL.
*/
QUrl QCborValue::toUrl(const QUrl &defaultValue) const {
    return t == Url && d ? QUrl(d->tagged.toString()) : defaultValue;
}

/*!
    Returns the regular expression of the value, or \a defaultValue if it is not one.
*/
QRegularExpression QCborValue::toRegularExpression(const QRegularExpression &defaultValue) const {
    return t == RegularExpression && d ? QRegularExpression(d->tagged.toString()) : defaultValue;
}

/*!
    Returns the UUID of the value, or \a defaultValue if it is not a UUID.
*/
QUuid QCborValue::toUuid(const QUuid &defaultValue) const {
    return t == Uuid && d ? QUuid::fromRfc4122(d->tagged.toByteArray()) : defaultValue;
}

/*!
    Returns the array of the value, or an empty array if it is not an array.
*/
QCborArray QCborValue::toArray() const { return toArray(QCborArray()); }

/*!
    \overload
    Returns the array of the value, or \a defaultValue if it is not an array.
*/
QCborArray QCborValue::toArray(const QCborArray &defaultValue) const {
    return t == Array && d ? d->array : defaultValue;
}

/*!
    Returns the map of the value, or an empty map if it is not a map.
*/
QCborMap QCborValue::toMap() const { return toMap(QCborMap()); }

/*!
    \overload
    Returns the map of the value, or \a defaultValue if it is not a map.
*/
QCborMap QCborValue::toMap(const QCborMap &defaultValue) const { return t == Map && d ? d->map : defaultValue; }

/*!
    Returns the value of \a key in the map, or \c undefined if the value is not a map or has no
    such key.
*/
const QCborValue QCborValue::operator[](const QString &key) const {
    return t == Map && d ? d->map.value(key) : QCborValue();
}

/*!
    \overload
    Returns the value of \a key in the map, or \c undefined.
*/
const QCborValue QCborValue::operator[](QLatin1StringView key) const {
    return t == Map && d ? d->map.value(key) : QCborValue();
}

/*!
    \overload
    Returns the element at \a key of an array, or the value of the integer \a key in a map, or
    \c undefined if there is none.
*/
const QCborValue QCborValue::operator[](qint64 key) const {
    if (t == Array && d)
        return key >= 0 && key < d->array.size() ? d->array.at(key) : QCborValue();
    if (t == Map && d)
        return d->map.value(key);
    return QCborValue();
}

/*!
    Returns a reference to the element at \a key. An array that is shorter grows, with
    \c undefined elements, unless \a key is far beyond its end, in which case it becomes a map
    with the indexes as keys; a map gets the key with an \c undefined value if it has none. A
    value that is neither becomes an empty array first.
*/
QCborValueRef QCborValue::operator[](qint64 key) {
    if (t != Array && t != Map) {
        *this = QCborValue(Array);
    }
    if (t == Array && (key < 0 || key >= payload().array.size() + 0x10000)) {
        // Too far beyond the end to grow: the array becomes a map of its indexes.
        QCborMap map;
        const QCborArray array = payload().array;
        for (qsizetype i = 0; i < array.size(); ++i)
            map.insert(static_cast<qint64>(i), array.at(i));
        *this = QCborValue(map);
    }
    if (t == Array)
        return payload().array[static_cast<qsizetype>(key)];
    return payload().map[key];
}

/*!
    \overload
    Returns a reference to the value of \a key in the map, which gets the key with an
    \c undefined value if it has none. An array becomes a map with its indexes as keys first, and
    any other value an empty map.
*/
QCborValueRef QCborValue::operator[](QLatin1StringView key) { return (*this)[QString(key)]; }

/*!
    \overload
    Returns a reference to the value of \a key in the map.
*/
QCborValueRef QCborValue::operator[](const QString &key) {
    if (t == Array) {
        QCborMap map;
        const QCborArray array = payload().array;
        for (qsizetype i = 0; i < array.size(); ++i)
            map.insert(static_cast<qint64>(i), array.at(i));
        *this = QCborValue(map);
    } else if (t != Map) {
        *this = QCborValue(Map);
    }
    return payload().map[key];
}

/*!
    Compares this value with \a other and returns a negative number if it comes first, 0 if they
    are equal and a positive number if it comes after. Values of different types are ordered by
    their type(); integers and floating-point numbers by their value, with NaN after all others;
    strings, byte arrays, arrays and maps first by their length and then element by element; tags
    by their number and then by what they apply to.

*/
int QCborValue::compare(const QCborValue &other) const {
    // The extended types are tags; they compare as the tags they are.
    const auto order = [](Type type) { return type >= DateTime ? int(Tag) : int(type); };
    if (order(t) != order(other.t))
        return order(t) < order(other.t) ? -1 : 1;
    switch (t) {
    case Integer:
        return sign(n, other.n);
    case Double: {
        const bool nanA = std::isnan(dbl), nanB = std::isnan(other.dbl);
        if (nanA || nanB)
            return nanA == nanB ? 0 : (nanA ? 1 : -1);
        return sign(dbl, other.dbl);
    }
    case ByteArray:
        return compareBytes(toByteArray(), other.toByteArray());
    case String:
        return compareBytes(toString().toUtf8(), other.toString().toUtf8());
    case Array:
        return toArray().compare(other.toArray());
    case Map:
        return toMap().compare(other.toMap());
    case Invalid:
        return 0;
    default:
        if (isTag()) {
            if (n != other.n)
                return sign(static_cast<quint64>(n), static_cast<quint64>(other.n));
            return taggedValue().compare(other.taggedValue());
        }
        return 0; // the simple types are their type
    }
}

/*!
    Returns the value of \a variant as a QCborValue: numbers, \c bool, strings, byte arrays, the
    extended types, lists, maps, JSON values and \c nullptr are converted; a null variant gives
    \c undefined. Any other type gives its conversion to a string if it has one, and
    \c undefined otherwise.

    \sa toVariant()
*/
QCborValue QCborValue::fromVariant(const QVariant &variant) {
    switch (variant.typeId()) {
    case QMetaType::UnknownType:
        return QCborValue();
    case QMetaType::Nullptr:
        return QCborValue(nullptr);
    case QMetaType::Bool:
        return QCborValue(variant.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Long:
    case QMetaType::LongLong:
        return QCborValue(static_cast<qint64>(variant.toLongLong()));
    case QMetaType::ULong:
    case QMetaType::ULongLong: {
        const qulonglong u = variant.toULongLong();
        if (u > static_cast<qulonglong>(std::numeric_limits<qint64>::max()))
            return QCborValue(static_cast<double>(u));
        return QCborValue(static_cast<qint64>(u));
    }
    case QMetaType::Float:
    case QMetaType::Double:
        return QCborValue(variant.toDouble());
    case QMetaType::QString:
        return QCborValue(variant.toString());
    case QMetaType::QStringList:
        return QCborArray::fromStringList(variant.toStringList());
    case QMetaType::QByteArray:
        return QCborValue(variant.toByteArray());
    case QMetaType::QDateTime:
        return QCborValue(variant.toDateTime());
    case QMetaType::QUrl:
        return QCborValue(variant.toUrl());
    case QMetaType::QUuid:
        return QCborValue(variant.toUuid());
    case QMetaType::QRegularExpression:
        return QCborValue(variant.toRegularExpression());
    case QMetaType::QVariantList:
        return QCborArray::fromVariantList(variant.toList());
    case QMetaType::QVariantMap:
        return QCborMap::fromVariantMap(variant.toMap());
    case QMetaType::QVariantHash:
        return QCborMap::fromVariantHash(variant.toHash());
    case QMetaType::QJsonValue:
    case QMetaType::QJsonObject:
    case QMetaType::QJsonArray:
        return fromJsonValue(variant.toJsonValue());
    case QMetaType::QCborValue:
        return variant.value<QCborValue>();
    case QMetaType::QCborArray:
        return variant.value<QCborArray>();
    case QMetaType::QCborMap:
        return variant.value<QCborMap>();
    case QMetaType::QCborSimpleType:
        return QCborValue(variant.value<QCborSimpleType>());
    default:
        if (variant.canConvert<QString>())
            return QCborValue(variant.toString());
        return QCborValue();
    }
}

/*!
    Returns the value as a QVariant: an integer as \c qint64, a floating-point number as
    \c double, strings and byte arrays as such, the extended types as QDateTime, QUrl,
    QRegularExpression and QUuid, an array as QVariantList, a map as QVariantMap (with keys that
    are not strings converted to text, as toJsonValue() does), \c null as \c nullptr and
    \c undefined as a null QVariant. A tag that is not known, and a simple type that has no
    meaning, are returned as the QCborValue itself and as a QCborSimpleType.

    \sa fromVariant()
*/
QVariant QCborValue::toVariant() const {
    switch (t) {
    case Integer:
        return QVariant::fromValue(static_cast<qlonglong>(n));
    case Double:
        return QVariant(dbl);
    case False:
    case True:
        return QVariant(t == True);
    case Null:
        return QVariant(nullptr);
    case Undefined:
    case Invalid:
        return QVariant();
    case ByteArray:
        return QVariant(toByteArray());
    case String:
        return QVariant(toString());
    case Array:
        return QVariant(toArray().toVariantList());
    case Map:
        return QVariant(toMap().toVariantMap());
    case DateTime:
        return QVariant(toDateTime());
    case Url:
        return QVariant(toUrl());
    case RegularExpression:
        return QVariant(toRegularExpression());
    case Uuid:
        return QVariant(toUuid());
    case Tag:
        return QVariant::fromValue(*this);
    default:
        return QVariant::fromValue(toSimpleType());
    }
}

/*!
    Returns the JSON value \a v as a QCborValue. A number that is a whole number of no more than
    53 bits becomes an Integer, and any other a Double.

    \sa toJsonValue()
*/
QCborValue QCborValue::fromJsonValue(const QJsonValue &v) {
    switch (v.type()) {
    case QJsonValue::Null:
        return QCborValue(nullptr);
    case QJsonValue::Bool:
        return QCborValue(v.toBool());
    case QJsonValue::Double: {
        const double d = v.toDouble();
        if (std::trunc(d) == d && std::fabs(d) <= 9007199254740992.0)
            return QCborValue(static_cast<qint64>(d));
        return QCborValue(d);
    }
    case QJsonValue::String:
        return QCborValue(v.toString());
    case QJsonValue::Array:
        return QCborArray::fromJsonArray(v.toArray());
    case QJsonValue::Object:
        return QCborMap::fromJsonObject(v.toObject());
    case QJsonValue::Undefined:
        break;
    }
    return QCborValue();
}

/*!
    Returns the value as a QJsonValue. Numbers, strings, \c bool, \c null, arrays and maps
    convert to their JSON kind; a floating-point number that is not finite, \c undefined and an
    invalid value become \c null. A byte array becomes text in Base64url, or in Base64 or
    hexadecimal if it is in a tag that asks for that. The extended types become their text: a
    date and time in ISO 8601, a URL, the pattern of a regular expression, and a UUID without
    braces. Another tag becomes what it applies to, and a simple type with no meaning the text
    \c{simple(n)}. The keys of a map that are not strings become their text.

    \sa fromJsonValue()
*/
QJsonValue QCborValue::toJsonValue() const {
    switch (t) {
    case Integer:
        return QJsonValue(n);
    case Double:
        return std::isfinite(dbl) ? QJsonValue(dbl) : QJsonValue(QJsonValue::Null);
    case False:
    case True:
        return QJsonValue(t == True);
    case String:
        return QJsonValue(toString());
    case ByteArray:
        return QJsonValue(QString::fromLatin1(
            toByteArray().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
    case Array:
        return QJsonValue(toArray().toJsonArray());
    case Map:
        return QJsonValue(toMap().toJsonObject());
    case DateTime:
    case Url:
    case RegularExpression:
        return QJsonValue(taggedValue().toString());
    case Uuid:
        return QJsonValue(toUuid().toString(QUuid::WithoutBraces));
    case Tag: {
        const QCborValue inner = taggedValue();
        if (inner.isByteArray()) {
            const QByteArray bytes = inner.toByteArray();
            switch (static_cast<quint64>(n)) {
            case quint64(QCborKnownTags::ExpectedBase64):
                return QJsonValue(QString::fromLatin1(bytes.toBase64()));
            case quint64(QCborKnownTags::ExpectedBase16):
                return QJsonValue(QString::fromLatin1(bytes.toHex()));
            default:
                break;
            }
        }
        return inner.toJsonValue();
    }
    case Null:
    case Undefined:
    case Invalid:
        return QJsonValue(QJsonValue::Null);
    default:
        return QJsonValue(QString("simple(%1)").arg(int(t - SimpleType)));
    }
}

namespace {

QCborValue readValue(QCborStreamReader &reader, int depth, bool *tooDeep) {
    using R = QCborStreamReader;
    switch (reader.type()) {
    case R::UnsignedInteger: {
        const quint64 u = reader.toUnsignedInteger();
        reader.next();
        if (u > static_cast<quint64>(std::numeric_limits<qint64>::max()))
            return QCborValue(static_cast<double>(u));
        return QCborValue(static_cast<qint64>(u));
    }
    case R::NegativeInteger: {
        const quint64 magnitude = static_cast<quint64>(reader.toNegativeInteger()) - 1; // -1 - value
        reader.next();
        if (magnitude > static_cast<quint64>(std::numeric_limits<qint64>::max()))
            return QCborValue(-1.0 - static_cast<double>(magnitude));
        return QCborValue(static_cast<qint64>(~magnitude));
    }
    case R::ByteString: {
        const QByteArray bytes = reader.readAllByteArray();
        return reader.lastError() == QCborError::NoError ? QCborValue(bytes) : QCborValue(QCborValue::Invalid);
    }
    case R::TextString: {
        const QString text = reader.readAllString();
        return reader.lastError() == QCborError::NoError ? QCborValue(text) : QCborValue(QCborValue::Invalid);
    }
    case R::Array:
    case R::Map: {
        if (depth >= kMaxDepth) {
            *tooDeep = true;
            reader.next();
            return QCborValue(QCborValue::Invalid);
        }
        const bool isMap = reader.isMap();
        QCborArray array;
        QCborMap map;
        reader.enterContainer();
        while (reader.hasNext() && reader.lastError() == QCborError::NoError) {
            QCborValue first = readValue(reader, depth + 1, tooDeep);
            if (!isMap) {
                array.append(first);
                continue;
            }
            if (!reader.hasNext())
                break; // a key without a value: the reader reports it
            map.insert(QCborMap::value_type(first, readValue(reader, depth + 1, tooDeep)));
        }
        if (reader.lastError() == QCborError::NoError)
            reader.leaveContainer();
        return isMap ? QCborValue(map) : QCborValue(array);
    }
    case R::Tag: {
        const QCborTag tag = reader.toTag();
        reader.next();
        if (depth >= kMaxDepth) {
            *tooDeep = true;
            reader.next();
            return QCborValue(QCborValue::Invalid);
        }
        const QCborValue inner = readValue(reader, depth + 1, tooDeep);
        // A tag without what it applies to is not a value.
        return inner.isInvalid() ? inner : QCborValue(tag, inner);
    }
    case R::SimpleType: {
        const QCborSimpleType st = reader.toSimpleType();
        reader.next();
        return QCborValue(st);
    }
    case R::HalfFloat: {
        const float f = reader.toFloat16();
        reader.next();
        return QCborValue(static_cast<double>(f));
    }
    case R::Float: {
        const float f = reader.toFloat();
        reader.next();
        return QCborValue(static_cast<double>(f));
    }
    case R::Double: {
        const double v = reader.toDouble();
        reader.next();
        return QCborValue(v);
    }
    case R::Invalid:
        break;
    }
    return QCborValue(QCborValue::Invalid);
}

} // namespace

/*!
    Reads the item that \a reader is at, with everything in it, and returns it. The reader is
    left at the next item. If the data is not valid, the value is what could be read, and the
    reader's lastError() says what went wrong.

    \sa toCbor()
*/
QCborValue QCborValue::fromCbor(QCborStreamReader &reader) {
    bool tooDeep = false;
    return readValue(reader, 0, &tooDeep);
}

/*!
    \overload
    Reads the CBOR in \a ba, which must hold one item and nothing after it. If \a error is not
    \c nullptr, it is set to the error and the offset where it was found, or to
    QCborError::NoError.
*/
QCborValue QCborValue::fromCbor(const QByteArray &ba, QCborParserError *error) {
    QCborStreamReader reader(ba);
    bool tooDeep = false;
    const QCborValue result = readValue(reader, 0, &tooDeep);
    QCborError found = reader.lastError();
    // After a whole value at the top level, whatever follows is garbage, even if it is not valid CBOR itself.
    const bool whole = !result.isInvalid() && reader.containerDepth() == 0;
    if (found == QCborError::NoError && tooDeep)
        found.c = QCborError::NestingTooDeep;
    else if ((found == QCborError::NoError && reader.isValid()) || (found != QCborError::NoError && whole))
        found.c = QCborError::GarbageAtEnd;
    if (error) {
        error->error = found;
        error->offset = found == QCborError::NoError ? 0 : reader.currentOffset();
    }
    return result;
}

/*!
    \overload
    Reads the CBOR in the \a len bytes at \a data. If \a error is not \c nullptr, it is set to the
    error that was found.
*/
QCborValue QCborValue::fromCbor(const char *data, qsizetype len, QCborParserError *error) {
    return fromCbor(QByteArray(data, len), error);
}

/*!
    \overload
    Reads the CBOR in the \a len bytes at \a data. If \a error is not \c nullptr, it is set to the
    error that was found.
*/
QCborValue QCborValue::fromCbor(const quint8 *data, qsizetype len, QCborParserError *error) {
    return fromCbor(QByteArray(reinterpret_cast<const char *>(data), len), error);
}

/*!
    Returns the value encoded in CBOR, with the options \a opt.

    \sa fromCbor()
*/
QByteArray QCborValue::toCbor(EncodingOptions opt) const {
    QByteArray result;
    QCborStreamWriter writer(&result);
    toCbor(writer, opt);
    return result;
}

/*!
    \overload
    Writes the value to \a writer, with the options \a opt.
*/
void QCborValue::toCbor(QCborStreamWriter &writer, EncodingOptions opt) const {
    switch (t) {
    case Integer:
        writer.append(n);
        return;
    case Double: {
        if (opt.testFlag(UseIntegers) && std::trunc(dbl) == dbl && std::fabs(dbl) < 9223372036854775808.0) {
            writer.append(static_cast<qint64>(dbl));
            return;
        }
        const auto single = static_cast<float>(dbl);
        const bool fitsFloat = std::isnan(dbl) || static_cast<double>(single) == dbl;
        if ((opt & UseFloat16) == UseFloat16 && fitsFloat) {
            const qfloat16 half(single);
            if (std::isnan(dbl) || static_cast<float>(half) == single) {
                writer.append(half);
                return;
            }
        }
        if (opt.testFlag(UseFloat) && fitsFloat) {
            writer.append(single);
            return;
        }
        writer.append(dbl);
        return;
    }
    case ByteArray:
        writer.append(toByteArray());
        return;
    case String:
        writer.append(QStringView(d ? d->text : QString()));
        return;
    case Array: {
        const QCborArray array = toArray();
        writer.startArray(static_cast<quint64>(array.size()));
        for (qsizetype i = 0; i < array.size(); ++i)
            array.at(i).toCbor(writer, opt);
        writer.endArray();
        return;
    }
    case Map: {
        const QCborMap map = toMap();
        writer.startMap(static_cast<quint64>(map.size()));
        if (opt.testFlag(SortKeysInMaps)) {
            // The canonical order: by the encoded key, shorter first.
            std::vector<std::pair<QByteArray, qsizetype>> keys;
            qsizetype i = 0;
            for (auto it = map.constBegin(); it != map.constEnd(); ++it, ++i)
                keys.emplace_back(it.key().toCbor(opt), i);
            std::stable_sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) {
                return compareBytes(a.first, b.first) < 0;
            });
            for (const auto &key : keys) {
                const auto it = map.constBegin() + key.second;
                it.key().toCbor(writer, opt);
                it.value().toCbor(writer, opt);
            }
        } else {
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                it.key().toCbor(writer, opt);
                it.value().toCbor(writer, opt);
            }
        }
        writer.endMap();
        return;
    }
    case Invalid:
        writer.append(QCborSimpleType::Undefined);
        return;
    default:
        if (isTag()) {
            writer.append(QCborTag(static_cast<quint64>(n)));
            taggedValue().toCbor(writer, opt);
            return;
        }
        writer.append(toSimpleType());
        return;
    }
}

namespace {

void diagnostic(const QCborValue &v, QCborValue::DiagnosticNotationOptions opts, int depth, QString &out) {
    const bool wrapped = opts.testFlag(QCborValue::LineWrapped);
    const auto newLine = [&](int level) {
        out += QChar(u'\n');
        out += QString(4 * level, QChar(u' '));
    };
    switch (v.type()) {
    case QCborValue::Integer:
        out += QString::number(v.toInteger());
        return;
    case QCborValue::Double: {
        const double d = v.toDouble();
        if (std::isnan(d))
            out += QString("NaN");
        else if (std::isinf(d))
            out += d > 0 ? QString("Infinity") : QString("-Infinity");
        else
            out += shortestDouble(d);
        if (opts.testFlag(QCborValue::ExtendedFormat))
            out += QString("_3");
        return;
    }
    case QCborValue::ByteArray:
        out += QString("h'") + QString::fromLatin1(v.toByteArray().toHex()) + QChar(u'\'');
        return;
    case QCborValue::String:
        out += quoted(v.toString());
        return;
    case QCborValue::Array: {
        const QCborArray array = v.toArray();
        out += QChar(u'[');
        for (qsizetype i = 0; i < array.size(); ++i) {
            if (i > 0)
                out += QChar(u',');
            if (wrapped)
                newLine(depth + 1);
            else if (i > 0)
                out += QChar(u' ');
            diagnostic(array.at(i), opts, depth + 1, out);
        }
        if (wrapped && !array.isEmpty())
            newLine(depth);
        out += QChar(u']');
        return;
    }
    case QCborValue::Map: {
        const QCborMap map = v.toMap();
        out += QChar(u'{');
        bool first = true;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!first)
                out += QChar(u',');
            if (wrapped)
                newLine(depth + 1);
            else if (!first)
                out += QChar(u' ');
            first = false;
            diagnostic(it.key().concrete(), opts, depth + 1, out);
            out += QString(": ");
            diagnostic(it.value().concrete(), opts, depth + 1, out);
        }
        if (wrapped && !map.isEmpty())
            newLine(depth);
        out += QChar(u'}');
        return;
    }
    case QCborValue::False:
        out += QString("false");
        return;
    case QCborValue::True:
        out += QString("true");
        return;
    case QCborValue::Null:
        out += QString("null");
        return;
    case QCborValue::Undefined:
        out += QString("undefined");
        return;
    case QCborValue::Invalid:
        out += QString("<invalid>");
        return;
    default:
        if (v.isTag()) {
            out += QString::number(static_cast<quint64>(v.tag())) + QChar(u'(');
            diagnostic(v.taggedValue(), opts, depth, out);
            out += QChar(u')');
            return;
        }
        out += QString("simple(%1)").arg(int(v.toSimpleType()));
        return;
    }
}

} // namespace

/*!
    Returns the value in the diagnostic notation of CBOR (RFC 8949 section 8), a text form for
    people to read, with the options \a opts:

    \code
    QCborValue(QCborArray{1, u"two"_s, QByteArray("\x03")}).toDiagnosticNotation(); // [1, "two", h'03']
    \endcode

    Floating-point numbers that are whole numbers end in \c{.0}, tags show their number before
    the value in parentheses, and simple types with no meaning appear as \c{simple(n)}.

    \sa toCbor()
*/
QString QCborValue::toDiagnosticNotation(DiagnosticNotationOptions opts) const {
    QString out;
    diagnostic(*this, opts, 0, out);
    return out;
}

/*!
    Returns the type of the value that the reference refers to.
*/
QCborValue::Type QCborValueConstRef::type() const { return v->type(); }

/*!
    Returns \c true if the value is an integer.
*/
bool QCborValueConstRef::isInteger() const { return v->isInteger(); }

/*!
    Returns \c true if the value is a byte array.
*/
bool QCborValueConstRef::isByteArray() const { return v->isByteArray(); }

/*!
    Returns \c true if the value is a string.
*/
bool QCborValueConstRef::isString() const { return v->isString(); }

/*!
    Returns \c true if the value is an array.
*/
bool QCborValueConstRef::isArray() const { return v->isArray(); }

/*!
    Returns \c true if the value is a map.
*/
bool QCborValueConstRef::isMap() const { return v->isMap(); }

/*!
    Returns \c true if the value is a tag, also one of the extended types.
*/
bool QCborValueConstRef::isTag() const { return v->isTag(); }

/*!
    Returns \c true if the value is \c false.
*/
bool QCborValueConstRef::isFalse() const { return v->isFalse(); }

/*!
    Returns \c true if the value is \c true.
*/
bool QCborValueConstRef::isTrue() const { return v->isTrue(); }

/*!
    Returns \c true if the value is \c false or \c true.
*/
bool QCborValueConstRef::isBool() const { return v->isBool(); }

/*!
    Returns \c true if the value is \c null.
*/
bool QCborValueConstRef::isNull() const { return v->isNull(); }

/*!
    Returns \c true if the value is \c undefined.
*/
bool QCborValueConstRef::isUndefined() const { return v->isUndefined(); }

/*!
    Returns \c true if the value is a floating-point number.
*/
bool QCborValueConstRef::isDouble() const { return v->isDouble(); }

/*!
    Returns \c true if the value is a date and time.
*/
bool QCborValueConstRef::isDateTime() const { return v->isDateTime(); }

/*!
    Returns \c true if the value is a URL.
*/
bool QCborValueConstRef::isUrl() const { return v->isUrl(); }

/*!
    Returns \c true if the value is a regular expression.
*/
bool QCborValueConstRef::isRegularExpression() const { return v->isRegularExpression(); }

/*!
    Returns \c true if the value is a UUID.
*/
bool QCborValueConstRef::isUuid() const { return v->isUuid(); }

/*!
    Returns \c true if the value is invalid.
*/
bool QCborValueConstRef::isInvalid() const { return v->isInvalid(); }

/*!
    Returns \c true if the value is an array or a map.
*/
bool QCborValueConstRef::isContainer() const { return v->isContainer(); }

/*!
    Returns \c true if the value is a simple type.
*/
bool QCborValueConstRef::isSimpleType() const { return v->isSimpleType(); }

/*!
    \overload
    Returns \c true if the value is the simple type \a st.
*/
bool QCborValueConstRef::isSimpleType(QCborSimpleType st) const { return v->isSimpleType(st); }

/*!
    Returns the simple type of the value, or \a defaultValue if it is not a simple type.
*/
QCborSimpleType QCborValueConstRef::toSimpleType(QCborSimpleType defaultValue) const { return v->toSimpleType(defaultValue); }

/*!
    Returns the integer of the value, or \a defaultValue if it is not an integer.
*/
qint64 QCborValueConstRef::toInteger(qint64 defaultValue) const { return v->toInteger(defaultValue); }

/*!
    Returns the boolean of the value, or \a defaultValue if it is not a boolean.
*/
bool QCborValueConstRef::toBool(bool defaultValue) const { return v->toBool(defaultValue); }

/*!
    Returns the value as a floating-point number, also an integer, or \a defaultValue if it is no number.
*/
double QCborValueConstRef::toDouble(double defaultValue) const { return v->toDouble(defaultValue); }

/*!
    Returns the tag of the value, or \a defaultValue if it is not a tag.
*/
QCborTag QCborValueConstRef::tag(QCborTag defaultValue) const { return v->tag(defaultValue); }

/*!
    Returns the value that the tag of the value applies to, or \a defaultValue if it is not a tag.
*/
QCborValue QCborValueConstRef::taggedValue(const QCborValue &defaultValue) const { return v->taggedValue(defaultValue); }

/*!
    Returns the byte array of the value, or \a defaultValue if it is not a byte array.
*/
QByteArray QCborValueConstRef::toByteArray(const QByteArray &defaultValue) const { return v->toByteArray(defaultValue); }

/*!
    Returns the string of the value, or \a defaultValue if it is not a string.
*/
QString QCborValueConstRef::toString(const QString &defaultValue) const { return v->toString(defaultValue); }

/*!
    Returns the date and time of the value, or \a defaultValue if it is not a date and time.
*/
QDateTime QCborValueConstRef::toDateTime(const QDateTime &defaultValue) const { return v->toDateTime(defaultValue); }

/*!
    Returns the URL of the value, or \a defaultValue if it is not a URL.
*/
QUrl QCborValueConstRef::toUrl(const QUrl &defaultValue) const { return v->toUrl(defaultValue); }

/*!
    Returns the regular expression of the value, or \a defaultValue if it is not one.
*/
QRegularExpression QCborValueConstRef::toRegularExpression(const QRegularExpression &defaultValue) const { return v->toRegularExpression(defaultValue); }

/*!
    Returns the UUID of the value, or \a defaultValue if it is not a UUID.
*/
QUuid QCborValueConstRef::toUuid(const QUuid &defaultValue) const { return v->toUuid(defaultValue); }

/*!
    Returns the array of the value, or an empty array if it is not an array.
*/
QCborArray QCborValueConstRef::toArray() const { return v->toArray(); }

/*!
    \overload
    Returns the array of the value, or \a defaultValue if it is not an array.
*/
QCborArray QCborValueConstRef::toArray(const QCborArray &defaultValue) const { return v->toArray(defaultValue); }

/*!
    Returns the map of the value, or an empty map if it is not a map.
*/
QCborMap QCborValueConstRef::toMap() const { return v->toMap(); }

/*!
    \overload
    Returns the map of the value, or \a defaultValue if it is not a map.
*/
QCborMap QCborValueConstRef::toMap(const QCborMap &defaultValue) const { return v->toMap(defaultValue); }

/*!
    Returns the value as a QVariant; see QCborValue::toVariant().
*/
QVariant QCborValueConstRef::toVariant() const { return v->toVariant(); }

/*!
    Returns the value as a QJsonValue; see QCborValue::toJsonValue().
*/
QJsonValue QCborValueConstRef::toJsonValue() const { return v->toJsonValue(); }

/*!
    Returns the value encoded in CBOR with the options \a opt.
*/
QByteArray QCborValueConstRef::toCbor(QCborValue::EncodingOptions opt) const { return v->toCbor(opt); }

/*!
    \overload
    Writes the value to \a writer with the options \a opt.
*/
void QCborValueConstRef::toCbor(QCborStreamWriter &writer, QCborValue::EncodingOptions opt) const { return v->toCbor(writer, opt); }

/*!
    Returns the value in the diagnostic notation of CBOR, with the options \a opt.
*/
QString QCborValueConstRef::toDiagnosticNotation(QCborValue::DiagnosticNotationOptions opt) const { return v->toDiagnosticNotation(opt); }

/*!
    Compares the value with \a other; see QCborValue::compare().
*/
int QCborValueConstRef::compare(const QCborValue &other) const { return v->compare(other); }

/*!
    Returns the value of \a key in the map that the reference refers to, or \c undefined.
*/
const QCborValue QCborValueConstRef::operator[](const QString &key) const { return static_cast<const QCborValue &>(*v)[key]; }

/*!
    \overload
    Returns the value of \a key in the map that the reference refers to, or \c undefined.
*/
const QCborValue QCborValueConstRef::operator[](QLatin1StringView key) const {
    return static_cast<const QCborValue &>(*v)[key];
}

/*!
    \overload
    Returns the element or value at \a key in the array or map that the reference refers to, or
    \c undefined.
*/
const QCborValue QCborValueConstRef::operator[](qint64 key) const { return static_cast<const QCborValue &>(*v)[key]; }

/*!
    Replaces the value that the reference refers to with \a other, and returns this reference.
*/
QCborValueRef &QCborValueRef::operator=(const QCborValue &other) {
    *v = other;
    return *this;
}

/*!
    \overload
    Replaces the value that the reference refers to with the value that \a other refers to, and
    returns this reference. The references go on referring to two values.
*/
QCborValueRef &QCborValueRef::operator=(const QCborValueRef &other) {
    const QCborValue copy = *other.v; // other may refer into this value
    *v = copy;
    return *this;
}

/*!
    Returns a reference to the element or value at \a key in the array or map that this reference
    refers to; see QCborValue::operator[]().
*/
QCborValueRef QCborValueRef::operator[](qint64 key) { return (*v)[key]; }

/*!
    \overload
    Returns a reference to the value of \a key in the map that this reference refers to.
*/
QCborValueRef QCborValueRef::operator[](QLatin1StringView key) { return (*v)[key]; }

/*!
    \overload
    Returns a reference to the value of \a key in the map that this reference refers to.
*/
QCborValueRef QCborValueRef::operator[](const QString &key) { return (*v)[key]; }
