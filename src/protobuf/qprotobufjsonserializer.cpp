// MIT License - see LICENSE
#include <QtProtobuf/qprotobufcodec_p.h>
#include <QtProtobuf/qprotobufjsonserializer.h>

#include <QtCore/qjsondocument.h>

#include <cmath>
#include <limits>

namespace QtProtobufPrivate::Json {

QJsonValue fromInt64(qint64 value) { return QJsonValue(QString::number(value)); }

QJsonValue fromUInt64(quint64 value) { return QJsonValue(QString::number(value)); }

QJsonValue fromDouble(double value) {
    if (std::isnan(value))
        return QJsonValue(QStringLiteral("NaN"));
    if (std::isinf(value))
        return QJsonValue(value > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity"));
    return QJsonValue(value);
}

QJsonValue fromFloat(float value) { return fromDouble(double(value)); }

QJsonValue fromBytes(const QByteArray &value) { return QJsonValue(QString::fromLatin1(value.toBase64())); }

QJsonValue fromMessage(const QProtobufMessage &message) {
    QJsonObject object;
    MessageAccess::toJson(message, object);
    return object;
}

bool toInteger(const QJsonValue &value, qint64 *out, qint64 minimum, qint64 maximum) {
    qint64 result = 0;
    if (value.isDouble()) {
        const double d = value.toDouble();
        if (!std::isfinite(d) || d != std::floor(d) || d < double(minimum) || d > double(maximum))
            return false;
        result = qint64(d);
    } else if (value.isString()) {
        bool ok = false;
        result = value.toString().trimmed().toLongLong(&ok);
        if (!ok)
            return false;
    } else {
        return false;
    }
    if (result < minimum || result > maximum)
        return false;
    *out = result;
    return true;
}

bool toUInt64(const QJsonValue &value, quint64 *out) {
    if (value.isDouble()) {
        const double d = value.toDouble();
        if (!std::isfinite(d) || d != std::floor(d) || d < 0 || d >= 18446744073709551616.0)
            return false;
        *out = quint64(d);
        return true;
    }
    if (value.isString()) {
        bool ok = false;
        const quint64 result = value.toString().trimmed().toULongLong(&ok);
        if (!ok)
            return false;
        *out = result;
        return true;
    }
    return false;
}

bool toDouble(const QJsonValue &value, double *out) {
    if (value.isDouble()) {
        *out = value.toDouble();
        return true;
    }
    if (!value.isString())
        return false;
    const QString text = value.toString();
    if (text == QLatin1String("NaN")) {
        *out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    if (text == QLatin1String("Infinity") || text == QLatin1String("-Infinity")) {
        *out = text.startsWith(QLatin1Char('-')) ? -std::numeric_limits<double>::infinity()
                                                 : std::numeric_limits<double>::infinity();
        return true;
    }
    bool ok = false;
    *out = text.toDouble(&ok);
    return ok;
}

bool toFloat(const QJsonValue &value, float *out) {
    double d = 0;
    if (!toDouble(value, &d))
        return false;
    if (std::isfinite(d) && std::abs(d) > double(std::numeric_limits<float>::max()))
        return false;
    *out = float(d);
    return true;
}

bool toBool(const QJsonValue &value, bool *out) {
    if (!value.isBool())
        return false;
    *out = value.toBool();
    return true;
}

bool toString(const QJsonValue &value, QString *out) {
    if (!value.isString())
        return false;
    *out = value.toString();
    return true;
}

bool toBytes(const QJsonValue &value, QByteArray *out) {
    if (!value.isString())
        return false;
    // Both the standard and the URL-safe alphabets of base64 are accepted, with or without padding.
    QByteArray text = value.toString().toLatin1();
    text.replace('-', '+');
    text.replace('_', '/');
    *out = QByteArray::fromBase64(text);
    return true;
}

bool toMessage(const QJsonValue &value, QProtobufMessage *message, QString *error) {
    if (!value.isObject()) {
        *error = QStringLiteral("a message must be a JSON object");
        return false;
    }
    return MessageAccess::fromJson(message, value.toObject(), error);
}

bool find(const QJsonObject &object, const char *json, const char *proto, QJsonValue *value) {
    QJsonValue found = object.value(QLatin1String(json));
    if (found.isUndefined())
        found = object.value(QLatin1String(proto));
    if (found.isUndefined() || found.isNull())
        return false;
    *value = found;
    return true;
}

} // namespace QtProtobufPrivate::Json

/*!
    \class QProtobufJsonSerializer
    \inmodule NtProtobuf
    \brief The QProtobufJsonSerializer class writes and reads messages in the JSON mapping of Protocol Buffers.
    \since 6.0

    The JSON is that of the proto3 mapping: each field under its JSON name, which is the field name
    in lowerCamelCase unless \c json_name says otherwise; 64-bit integers as strings; bytes in
    base64; enum values by name; maps as objects; and fields that hold their default value left
    out. When reading, the field name of the \c .proto file is accepted too, integers may be numbers
    or strings, and unknown names are ignored.

    \code
    QProtobufJsonSerializer json;
    qDebug() << person.serialize(&json);   // {"name":"Ada","id":1815}
    \endcode

    \differences
    The well-known types, such as \c google.protobuf.Timestamp, have no special JSON form yet.
    \enddifferences

    \sa QProtobufSerializer
*/

/*!
    Constructs a JSON serializer.
*/
QProtobufJsonSerializer::QProtobufJsonSerializer() = default;

/*!
    Destroys the serializer.
*/
QProtobufJsonSerializer::~QProtobufJsonSerializer() = default;

/*!
    \reimp
*/
QAbstractProtobufSerializer::Error QProtobufJsonSerializer::lastError() const { return m_error; }

/*!
    \reimp
*/
QString QProtobufJsonSerializer::lastErrorString() const { return m_errorString; }

QByteArray QProtobufJsonSerializer::serializeMessage(const QProtobufMessage *message) const {
    QJsonObject object;
    QtProtobufPrivate::MessageAccess::toJson(*message, object);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool QProtobufJsonSerializer::deserializeMessage(QProtobufMessage *message, QByteArrayView data) const {
    m_error = Error::None;
    m_errorString.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_error = Error::InvalidFormat;
        m_errorString = parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                                     : QStringLiteral("not a JSON object");
        return false;
    }
    QtProtobufPrivate::MessageAccess::clear(message);
    QString error;
    if (!QtProtobufPrivate::MessageAccess::fromJson(message, document.object(), &error)) {
        m_error = Error::InvalidFormat;
        m_errorString = error;
        return false;
    }
    return true;
}
