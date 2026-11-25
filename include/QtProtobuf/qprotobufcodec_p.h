// MIT License - see LICENSE
//
// The encoding that the code ntprotobufgen generates calls: the binary wire format (Writer and Reader) and the values
// of the JSON mapping of proto3. It is not part of the API of Nt Protobuf and may change with the generator.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qhash.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtProtobuf/qabstractprotobufserializer.h>
#include <QtProtobuf/qprotobufmessage.h>
#include <QtProtobuf/qtprotobuftypes.h>

#include <cstring>
#include <memory>
#include <type_traits>

namespace QtProtobufPrivate {

// How a scalar is encoded on the wire.
enum class Scalar {
    Int32,
    Int64,
    UInt32,
    UInt64,
    SInt32,
    SInt64,
    Fixed32,
    Fixed64,
    SFixed32,
    SFixed64,
    Bool,
    Float,
    Double
};

class Writer {
public:
    explicit Writer(QByteArray *out) : m_out(out) {}

    void varint(quint64 value);
    void tag(int field, QtProtobuf::WireTypes type);
    // A scalar of kind without its tag.
    void value(Scalar kind, qint64 value);
    void value(Scalar kind, float value);
    void value(Scalar kind, double value);
    // A scalar field, left out when it has its default value unless force is set (for a field with presence).
    void scalar(int field, Scalar kind, qint64 value, bool force = false);
    void scalar(int field, Scalar kind, float value, bool force = false);
    void scalar(int field, Scalar kind, double value, bool force = false);
    void string(int field, const QString &value, bool force = false);
    void bytes(int field, const QByteArray &value, bool force = false);
    void message(int field, const QProtobufMessage &message);
    void raw(const QByteArray &bytes) { m_out->append(bytes); }

    template <typename T> void repeated(int field, const QList<T> &list, Scalar kind, bool packed) {
        if (list.isEmpty())
            return;
        if (packed) {
            QByteArray body;
            Writer inner(&body);
            for (const T &item : list)
                inner.value(kind, convert(item));
            tag(field, QtProtobuf::WireTypes::LengthDelimited);
            varint(quint64(body.size()));
            m_out->append(body);
        } else {
            for (const T &item : list) {
                tag(field, wireType(kind));
                value(kind, convert(item));
            }
        }
    }

    static QtProtobuf::WireTypes wireType(Scalar kind);

private:
    template <typename T> static auto convert(const T &item) {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
            return item;
        else if constexpr (std::is_enum_v<T>)
            return qint64(static_cast<std::underlying_type_t<T>>(item));
        else
            return qint64(item);
    }

    QByteArray *m_out;
};

class Reader {
public:
    Reader(QByteArrayView data, bool preserveUnknown) : m_data(data), m_preserve(preserveUnknown) {}

    // Moves to the next field; false at the end of the data or on an error (see failed()).
    bool next(int *field, QtProtobuf::WireTypes *wire);
    bool failed() const { return m_error != QAbstractProtobufSerializer::Error::None; }
    QAbstractProtobufSerializer::Error error() const { return m_error; }
    QString errorString() const { return m_errorString; }
    bool fail(QAbstractProtobufSerializer::Error error, const QString &message);

    // The value of the current field; a wrong wire type is an error.
    qint64 integer(Scalar kind);
    float floatValue();
    double doubleValue();
    bool boolean() { return integer(Scalar::Bool) != 0; }
    QString string();
    QByteArray bytes();
    // Merges the current field, a nested message, into message.
    bool message(QProtobufMessage *message);
    // A reader over the current length-delimited field, for an entry of a map.
    Reader entry();
    // Keeps the current field, which the class does not know, with message, or skips it.
    void unknown(QProtobufMessage *message);
    void skip();

    template <typename T> void repeated(QList<T> *list, Scalar kind) {
        const auto add = [&](Reader &r) {
            if constexpr (std::is_same_v<T, float>)
                list->append(r.readFloat());
            else if constexpr (std::is_same_v<T, double>)
                list->append(r.readDouble());
            else if constexpr (std::is_enum_v<T>)
                list->append(T(r.readInteger(kind)));
            else if constexpr (std::is_same_v<T, bool>)
                list->append(r.readInteger(kind) != 0);
            else
                list->append(T(r.readInteger(kind)));
        };
        if (m_wire == QtProtobuf::WireTypes::LengthDelimited) { // packed
            Reader inner = entry();
            while (!inner.failed() && inner.m_pos < inner.m_data.size()) {
                inner.m_wire = Writer::wireType(kind);
                add(inner);
            }
            if (inner.failed())
                fail(inner.error(), inner.errorString());
        } else {
            add(*this);
        }
    }

private:
    bool readVarint(quint64 *value);
    bool readFixed(int bytes, quint64 *value);
    bool expect(QtProtobuf::WireTypes wire);
    qint64 readInteger(Scalar kind);
    float readFloat();
    double readDouble();
    bool readLength(QByteArrayView *payload);

    QByteArrayView m_data;
    qsizetype m_pos = 0;
    qsizetype m_fieldStart = 0;
    int m_field = 0;
    QtProtobuf::WireTypes m_wire = QtProtobuf::WireTypes::Unknown;
    bool m_preserve;
    QAbstractProtobufSerializer::Error m_error = QAbstractProtobufSerializer::Error::None;
    QString m_errorString;
};

// The values of the JSON mapping of proto3.
namespace Json {
QJsonValue fromInt64(qint64 value);   // as a string
QJsonValue fromUInt64(quint64 value); // as a string
QJsonValue fromFloat(float value);    // NaN and infinities as strings
QJsonValue fromDouble(double value);
QJsonValue fromBytes(const QByteArray &value); // base64
QJsonValue fromMessage(const QProtobufMessage &message);
bool toInteger(const QJsonValue &value, qint64 *out, qint64 minimum, qint64 maximum);
bool toUInt64(const QJsonValue &value, quint64 *out);
bool toFloat(const QJsonValue &value, float *out);
bool toDouble(const QJsonValue &value, double *out);
bool toBool(const QJsonValue &value, bool *out);
bool toString(const QJsonValue &value, QString *out);
bool toBytes(const QJsonValue &value, QByteArray *out);
bool toMessage(const QJsonValue &value, QProtobufMessage *message, QString *error);
// The value of the field named json or, failing that, proto in object; false if neither is there or it is null.
bool find(const QJsonObject &object, const char *json, const char *proto, QJsonValue *value);
} // namespace Json

struct MessageAccess {
    static void clear(QProtobufMessage *m) { m->ntClear(); }
    static void encode(const QProtobufMessage &m, Writer &w) { m.ntEncode(w); }
    static bool decode(QProtobufMessage *m, Reader &r) { return m->ntDecode(r); }
    static void toJson(const QProtobufMessage &m, QJsonObject &o) { m.ntToJson(o); }
    static bool fromJson(QProtobufMessage *m, const QJsonObject &o, QString *e) { return m->ntFromJson(o, e); }
    static void addUnknown(QProtobufMessage *m, qint32 field, const QByteArray &raw) {
        m->m_unknown[field].append(raw);
    }
    static const QHash<qint32, QList<QByteArray>> &unknown(const QProtobufMessage &m) { return m.m_unknown; }
};

// The storage of a singular message field: made when it is first written, so that a message can hold one of its own
// type, and copied with the message that holds it.
template <typename T> class MessageField {
public:
    MessageField() = default;
    MessageField(const MessageField &other) : m_value(other.m_value ? new T(*other.m_value) : nullptr) {}
    MessageField &operator=(const MessageField &other) {
        if (this != &other)
            m_value.reset(other.m_value ? new T(*other.m_value) : nullptr);
        return *this;
    }
    MessageField(MessageField &&other) noexcept = default;
    MessageField &operator=(MessageField &&other) noexcept = default;
    ~MessageField() = default;

    const T &get() const {
        if (m_value)
            return *m_value;
        static const T empty;
        return empty;
    }
    T &mut() {
        if (!m_value)
            m_value.reset(new T);
        return *m_value;
    }
    void reset() { m_value.reset(); }
    bool operator==(const MessageField &other) const { return get() == other.get(); }

private:
    std::unique_ptr<T> m_value;
};

// Registers a generated message class, so that QProtobufMessage::constructByName() can make it.
struct MessageRegistration {
    explicit MessageRegistration(const QProtobufPropertyOrdering *ordering);
};

} // namespace QtProtobufPrivate
