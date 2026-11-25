// MIT License - see LICENSE
#include <QtProtobuf/qprotobufcodec_p.h>
#include <QtProtobuf/qprotobufserializer.h>

#include <QtCore/qmap.h>

#include <cmath>
#include <limits>

namespace QtProtobufPrivate {

namespace {

constexpr qsizetype kMaximumSize = std::numeric_limits<qint32>::max(); // messages are under 2 GiB

quint64 zigzag(qint64 value) { return (quint64(value) << 1) ^ quint64(value >> 63); }
qint64 unzigzag(quint64 value) { return qint64(value >> 1) ^ -qint64(value & 1); }

template <typename T> quint64 bitsOf(T value) {
    if constexpr (sizeof(T) == 4) {
        quint32 bits = 0;
        std::memcpy(&bits, &value, 4);
        return bits;
    } else {
        quint64 bits = 0;
        std::memcpy(&bits, &value, 8);
        return bits;
    }
}

} // namespace

QtProtobuf::WireTypes Writer::wireType(Scalar kind) {
    switch (kind) {
    case Scalar::Fixed32:
    case Scalar::SFixed32:
    case Scalar::Float:
        return QtProtobuf::WireTypes::Fixed32;
    case Scalar::Fixed64:
    case Scalar::SFixed64:
    case Scalar::Double:
        return QtProtobuf::WireTypes::Fixed64;
    default:
        return QtProtobuf::WireTypes::Varint;
    }
}

void Writer::varint(quint64 value) {
    char buffer[10];
    int n = 0;
    do {
        auto byte = static_cast<unsigned char>(value & 0x7f);
        value >>= 7;
        if (value)
            byte |= 0x80;
        buffer[n++] = static_cast<char>(byte);
    } while (value);
    m_out->append(buffer, n);
}

void Writer::tag(int field, QtProtobuf::WireTypes type) { varint((quint64(quint32(field)) << 3) | quint64(type)); }

void Writer::value(Scalar kind, qint64 value) {
    switch (kind) {
    case Scalar::Int32:
        varint(quint64(qint64(qint32(value)))); // a negative int32 takes ten bytes, as in every encoder
        break;
    case Scalar::Int64:
    case Scalar::UInt64:
        varint(quint64(value));
        break;
    case Scalar::UInt32:
        varint(quint64(quint32(value)));
        break;
    case Scalar::SInt32:
        varint(quint64(quint32(zigzag(qint32(value)))));
        break;
    case Scalar::SInt64:
        varint(zigzag(value));
        break;
    case Scalar::Bool:
        varint(value ? 1 : 0);
        break;
    case Scalar::Fixed32:
    case Scalar::SFixed32: {
        const auto bits = quint32(value);
        char bytes[4];
        for (int i = 0; i < 4; ++i)
            bytes[i] = static_cast<char>((bits >> (8 * i)) & 0xff);
        m_out->append(bytes, 4);
        break;
    }
    case Scalar::Fixed64:
    case Scalar::SFixed64: {
        const auto bits = quint64(value);
        char bytes[8];
        for (int i = 0; i < 8; ++i)
            bytes[i] = static_cast<char>((bits >> (8 * i)) & 0xff);
        m_out->append(bytes, 8);
        break;
    }
    case Scalar::Float:
        this->value(kind, float(value));
        break;
    case Scalar::Double:
        this->value(kind, double(value));
        break;
    }
}

void Writer::value(Scalar kind, float value) {
    if (kind == Scalar::Double) {
        this->value(kind, double(value));
        return;
    }
    this->value(Scalar::Fixed32, qint64(bitsOf(value)));
}

void Writer::value(Scalar kind, double value) {
    if (kind == Scalar::Float) {
        this->value(kind, float(value));
        return;
    }
    this->value(Scalar::Fixed64, qint64(bitsOf(value)));
}

void Writer::scalar(int field, Scalar kind, qint64 value, bool force) {
    if (!force && value == 0)
        return;
    tag(field, wireType(kind));
    this->value(kind, value);
}

void Writer::scalar(int field, Scalar kind, float value, bool force) {
    if (!force && bitsOf(value) == 0) // -0.0 is not the default and is written
        return;
    tag(field, wireType(kind));
    this->value(kind, value);
}

void Writer::scalar(int field, Scalar kind, double value, bool force) {
    if (!force && bitsOf(value) == 0)
        return;
    tag(field, wireType(kind));
    this->value(kind, value);
}

void Writer::string(int field, const QString &value, bool force) {
    if (!force && value.isEmpty())
        return;
    bytes(field, value.toUtf8(), true);
}

void Writer::bytes(int field, const QByteArray &value, bool force) {
    if (!force && value.isEmpty())
        return;
    tag(field, QtProtobuf::WireTypes::LengthDelimited);
    varint(quint64(value.size()));
    m_out->append(value);
}

void Writer::message(int field, const QProtobufMessage &message) {
    QByteArray body;
    Writer inner(&body);
    MessageAccess::encode(message, inner);
    // The fields that the message did not know go back as they came, in the order of their numbers.
    const QHash<qint32, QList<QByteArray>> &unknown = MessageAccess::unknown(message);
    QMap<qint32, QList<QByteArray>> ordered;
    for (auto it = unknown.cbegin(); it != unknown.cend(); ++it)
        ordered.insert(it.key(), it.value());
    for (const QList<QByteArray> &fields : ordered)
        for (const QByteArray &raw : fields)
            body.append(raw);
    if (field == 0) { // the top-level message: no tag
        m_out->append(body);
        return;
    }
    bytes(field, body, true);
}

bool Reader::fail(QAbstractProtobufSerializer::Error error, const QString &message) {
    if (!failed()) {
        m_error = error;
        m_errorString = message;
    }
    return false;
}

bool Reader::readVarint(quint64 *value) {
    quint64 result = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        if (m_pos >= m_data.size())
            return fail(QAbstractProtobufSerializer::Error::UnexpectedEndOfStream, QStringLiteral("truncated varint"));
        const auto byte = static_cast<unsigned char>(m_data[m_pos++]);
        if (shift < 64)
            result |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            *value = result;
            return true;
        }
    }
    return fail(QAbstractProtobufSerializer::Error::InvalidFormat, QStringLiteral("varint longer than ten bytes"));
}

bool Reader::readFixed(int bytes, quint64 *value) {
    if (m_data.size() - m_pos < bytes)
        return fail(QAbstractProtobufSerializer::Error::UnexpectedEndOfStream,
                    QStringLiteral("truncated fixed-size value"));
    quint64 result = 0;
    for (int i = 0; i < bytes; ++i)
        result |= quint64(static_cast<unsigned char>(m_data[m_pos + i])) << (8 * i);
    m_pos += bytes;
    *value = result;
    return true;
}

bool Reader::readLength(QByteArrayView *payload) {
    quint64 length = 0;
    if (!readVarint(&length))
        return false;
    if (length > quint64(m_data.size() - m_pos))
        return fail(QAbstractProtobufSerializer::Error::UnexpectedEndOfStream,
                    QStringLiteral("field %1 is longer than the data").arg(m_field));
    *payload = m_data.mid(m_pos, qsizetype(length));
    m_pos += qsizetype(length);
    return true;
}

bool Reader::next(int *field, QtProtobuf::WireTypes *wire) {
    if (failed() || m_pos >= m_data.size())
        return false;
    if (m_data.size() > kMaximumSize)
        return fail(QAbstractProtobufSerializer::Error::InvalidFormat, QStringLiteral("the data is 2 GiB or more"));
    m_fieldStart = m_pos;
    quint64 tag = 0;
    if (!readVarint(&tag))
        return false;
    const auto type = int(tag & 7);
    const quint64 number = tag >> 3;
    if (number == 0 || number > 536870911)
        return fail(QAbstractProtobufSerializer::Error::InvalidHeader, QStringLiteral("invalid field number"));
    if (type == 3 || type == 4)
        return fail(QAbstractProtobufSerializer::Error::InvalidHeader, QStringLiteral("groups are not supported"));
    if (type > 5)
        return fail(QAbstractProtobufSerializer::Error::InvalidHeader,
                    QStringLiteral("invalid wire type %1").arg(type));
    m_field = int(number);
    m_wire = QtProtobuf::WireTypes(type);
    *field = m_field;
    *wire = m_wire;
    return true;
}

bool Reader::expect(QtProtobuf::WireTypes wire) {
    if (m_wire == wire)
        return true;
    return fail(QAbstractProtobufSerializer::Error::InvalidHeader,
                QStringLiteral("field %1 has wire type %2, not %3").arg(m_field).arg(int(m_wire)).arg(int(wire)));
}

qint64 Reader::readInteger(Scalar kind) {
    quint64 raw = 0;
    switch (Writer::wireType(kind)) {
    case QtProtobuf::WireTypes::Fixed32:
        if (!readFixed(4, &raw))
            return 0;
        return kind == Scalar::SFixed32 ? qint64(qint32(quint32(raw))) : qint64(raw);
    case QtProtobuf::WireTypes::Fixed64:
        if (!readFixed(8, &raw))
            return 0;
        return qint64(raw);
    default:
        break;
    }
    if (!readVarint(&raw))
        return 0;
    switch (kind) {
    case Scalar::Int32:
        return qint32(quint32(raw));
    case Scalar::UInt32:
        return quint32(raw);
    case Scalar::SInt32:
        return qint32(unzigzag(quint32(raw)));
    case Scalar::SInt64:
        return unzigzag(raw);
    case Scalar::Bool:
        return raw != 0 ? 1 : 0;
    default:
        return qint64(raw);
    }
}

float Reader::readFloat() {
    quint64 raw = 0;
    if (!readFixed(4, &raw))
        return 0;
    const auto bits = quint32(raw);
    float value = 0;
    std::memcpy(&value, &bits, 4);
    return value;
}

double Reader::readDouble() {
    quint64 raw = 0;
    if (!readFixed(8, &raw))
        return 0;
    double value = 0;
    std::memcpy(&value, &raw, 8);
    return value;
}

qint64 Reader::integer(Scalar kind) {
    if (!expect(Writer::wireType(kind)))
        return 0;
    return readInteger(kind);
}

float Reader::floatValue() { return expect(QtProtobuf::WireTypes::Fixed32) ? readFloat() : 0.0f; }

double Reader::doubleValue() { return expect(QtProtobuf::WireTypes::Fixed64) ? readDouble() : 0.0; }

QByteArray Reader::bytes() {
    QByteArrayView payload;
    if (!expect(QtProtobuf::WireTypes::LengthDelimited) || !readLength(&payload))
        return QByteArray();
    return payload.toByteArray();
}

QString Reader::string() { return QString::fromUtf8(bytes()); }

bool Reader::message(QProtobufMessage *message) {
    Reader inner = entry();
    if (failed())
        return false;
    if (!MessageAccess::decode(message, inner))
        return fail(inner.error(), inner.errorString());
    return true;
}

Reader Reader::entry() {
    QByteArrayView payload;
    if (!expect(QtProtobuf::WireTypes::LengthDelimited) || !readLength(&payload))
        return Reader(QByteArrayView(), m_preserve);
    return Reader(payload, m_preserve);
}

void Reader::skip() {
    quint64 ignored = 0;
    QByteArrayView payload;
    switch (m_wire) {
    case QtProtobuf::WireTypes::Varint:
        readVarint(&ignored);
        break;
    case QtProtobuf::WireTypes::Fixed64:
        readFixed(8, &ignored);
        break;
    case QtProtobuf::WireTypes::Fixed32:
        readFixed(4, &ignored);
        break;
    case QtProtobuf::WireTypes::LengthDelimited:
        readLength(&payload);
        break;
    default:
        fail(QAbstractProtobufSerializer::Error::InvalidHeader, QStringLiteral("cannot skip field %1").arg(m_field));
        break;
    }
}

void Reader::unknown(QProtobufMessage *message) {
    skip();
    if (!failed() && m_preserve && message)
        MessageAccess::addUnknown(message, m_field, m_data.mid(m_fieldStart, m_pos - m_fieldStart).toByteArray());
}

} // namespace QtProtobufPrivate

/*!
    \class QAbstractProtobufSerializer
    \inmodule NtProtobuf
    \brief The QAbstractProtobufSerializer class is the base of the serializers of Protocol Buffers messages.
    \since 6.0

    \differences
    The serializers call code that ntprotobufgen generates for each message, so a serializer for
    another format cannot be written by subclassing.
    \enddifferences

    \sa QProtobufSerializer, QProtobufJsonSerializer
*/

/*!
    \enum QAbstractProtobufSerializer::Error

    This enum describes why deserializing failed.

    \value None No error.
    \value InvalidHeader A field has an invalid number or wire type, or is a group.
    \value UnknownType A field has a type that the serializer cannot read.
    \value UnexpectedEndOfStream The data ends inside a field.
    \value InvalidFormat The data is not a message: a varint is too long, the data is too large, or a JSON value
           does not fit its field.
*/

/*!
    Constructs a serializer.
*/
QAbstractProtobufSerializer::QAbstractProtobufSerializer() = default;

/*!
    Destroys the serializer.
*/
QAbstractProtobufSerializer::~QAbstractProtobufSerializer() = default;

/*!
    Returns \a message, which must not be \c nullptr, in the format of the serializer.
*/
QByteArray QAbstractProtobufSerializer::serialize(const QProtobufMessage *message) const {
    return message ? serializeMessage(message) : QByteArray();
}

/*!
    Replaces \a message, which must not be \c nullptr, with the one read from \a data. Returns
    \c false on an error, which lastError() and lastErrorString() describe.
*/
bool QAbstractProtobufSerializer::deserialize(QProtobufMessage *message, QByteArrayView data) const {
    return message && deserializeMessage(message, data);
}

/*!
    \fn QAbstractProtobufSerializer::Error QAbstractProtobufSerializer::lastError() const

    Returns the error of the last deserialize(), or \c Error::None.
*/

/*!
    \fn QString QAbstractProtobufSerializer::lastErrorString() const

    Returns a description of the error of the last deserialize(), or an empty string.
*/

/*!
    \class QProtobufSerializer
    \inmodule NtProtobuf
    \brief The QProtobufSerializer class writes and reads messages in the binary format of Protocol Buffers.
    \since 6.0

    The output is what every implementation of Protocol Buffers reads: scalars that hold their
    default value are left out, numeric lists of proto3 are packed, and fields that the class does
    not know are kept and written back.

    \code
    QProtobufSerializer serializer;
    const QByteArray bytes = message.serialize(&serializer);
    \endcode

    \differences
    Groups of proto2 are not supported: data with a group fails with \c Error::InvalidHeader. The
    largest message is 2 GiB less a byte; \c QT_PROTOBUF_MAXIMUM_INPUT_SIZE is not read.
    \enddifferences

    \sa QProtobufJsonSerializer
*/

/*!
    Constructs a serializer that keeps unknown fields.
*/
QProtobufSerializer::QProtobufSerializer() = default;

/*!
    Destroys the serializer.
*/
QProtobufSerializer::~QProtobufSerializer() = default;

/*!
    \reimp
*/
QAbstractProtobufSerializer::Error QProtobufSerializer::lastError() const { return m_error; }

/*!
    \reimp
*/
QString QProtobufSerializer::lastErrorString() const { return m_errorString; }

/*!
    Sets whether fields that a message class does not know are kept, to be written again, to
    \a preserveUnknownFields. They are kept by default.
*/
void QProtobufSerializer::shouldPreserveUnknownFields(bool preserveUnknownFields) {
    m_preserveUnknown = preserveUnknownFields;
}

QByteArray QProtobufSerializer::serializeMessage(const QProtobufMessage *message) const {
    QByteArray out;
    QtProtobufPrivate::Writer writer(&out);
    writer.message(0, *message);
    return out;
}

bool QProtobufSerializer::deserializeMessage(QProtobufMessage *message, QByteArrayView data) const {
    m_error = Error::None;
    m_errorString.clear();
    QtProtobufPrivate::MessageAccess::clear(message);
    QtProtobufPrivate::Reader reader(data, m_preserveUnknown);
    if (QtProtobufPrivate::MessageAccess::decode(message, reader) && !reader.failed())
        return true;
    m_error = reader.failed() ? reader.error() : Error::InvalidFormat;
    m_errorString = reader.errorString();
    return false;
}
