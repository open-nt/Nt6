// MIT License - see LICENSE
#include <QtProtobuf/qabstractprotobufserializer.h>
#include <QtProtobuf/qprotobufcodec_p.h>
#include <QtProtobuf/qprotobufmessage.h>
#include <QtProtobuf/qtprotobuftypes.h>

#include <QtCore/qjsonobject.h>

#include <algorithm>
#include <mutex>

/*!
    \module NtProtobuf
    \title Nt Protobuf C++ Classes
    \brief Classes for Protocol Buffers messages, made from .proto files by ntprotobufgen.
    \cmaketarget Nt::Protobuf

    Nt Protobuf is a subset of Qt Protobuf. ntprotobufgen reads \c .proto files and writes a C++
    class for each message, which QProtobufSerializer writes in the binary format of Protocol
    Buffers and QProtobufJsonSerializer in its JSON mapping. The CMake function \c qt_add_protobuf()
    runs the generator.

    See \l{Using Nt Protobuf} for a walk through.
*/

namespace QtProtobufPrivate {

namespace {

struct Registry {
    std::mutex mutex;
    QHash<QString, const QProtobufPropertyOrdering *> messages;
};

Registry &registry() {
    static Registry r;
    return r;
}

} // namespace

MessageRegistration::MessageRegistration(const QProtobufPropertyOrdering *ordering) {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    r.messages.insert(QString::fromUtf8(ordering->messageFullName), ordering);
}

} // namespace QtProtobufPrivate

/*!
    \namespace QtProtobuf
    \inmodule NtProtobuf
    \brief The QtProtobuf namespace holds the scalar types of Protocol Buffers.

    Each scalar type of a \c .proto file has a type of the same name here, such as
    QtProtobuf::sint32, and a list of it, such as QtProtobuf::sint32List. The name tells how the
    value is encoded: \c int32 and \c int64 as variable-length integers, \c sint32 and \c sint64
    with zig-zag encoding, which keeps small negative numbers short, and \c fixed32, \c fixed64,
    \c sfixed32 and \c sfixed64 in four or eight bytes.

    \differences
    The types are plain aliases of the integers of \c <cstdint>, so QtProtobuf::int32 and
    QtProtobuf::sint32 are the same type; Qt makes each a distinct type. The encoding is chosen
    by the generated code, which knows the type of each field.
    \enddifferences
*/

/*!
    \typedef QtProtobuf::int32

    A signed 32-bit integer, encoded as a variable-length integer; a negative value takes ten bytes.
*/

/*!
    \typedef QtProtobuf::int64

    A signed 64-bit integer, encoded as a variable-length integer.
*/

/*!
    \typedef QtProtobuf::uint32

    An unsigned 32-bit integer, encoded as a variable-length integer.
*/

/*!
    \typedef QtProtobuf::uint64

    An unsigned 64-bit integer, encoded as a variable-length integer.
*/

/*!
    \typedef QtProtobuf::sint32

    A signed 32-bit integer, zig-zag encoded so that small negative values stay short.
*/

/*!
    \typedef QtProtobuf::sint64

    A signed 64-bit integer, zig-zag encoded.
*/

/*!
    \typedef QtProtobuf::fixed32

    An unsigned 32-bit integer, encoded in four bytes.
*/

/*!
    \typedef QtProtobuf::fixed64

    An unsigned 64-bit integer, encoded in eight bytes.
*/

/*!
    \typedef QtProtobuf::sfixed32

    A signed 32-bit integer, encoded in four bytes.
*/

/*!
    \typedef QtProtobuf::sfixed64

    A signed 64-bit integer, encoded in eight bytes.
*/

/*!
    \typedef QtProtobuf::boolean

    A bool.
*/

/*!
    \typedef QtProtobuf::int32List

    A list of QtProtobuf::int32, the type of a repeated int32 field.
*/

/*!
    \typedef QtProtobuf::int64List

    A list of QtProtobuf::int64, the type of a repeated int64 field.
*/

/*!
    \typedef QtProtobuf::uint32List

    A list of QtProtobuf::uint32, the type of a repeated uint32 field.
*/

/*!
    \typedef QtProtobuf::uint64List

    A list of QtProtobuf::uint64, the type of a repeated uint64 field.
*/

/*!
    \typedef QtProtobuf::sint32List

    A list of QtProtobuf::sint32, the type of a repeated sint32 field.
*/

/*!
    \typedef QtProtobuf::sint64List

    A list of QtProtobuf::sint64, the type of a repeated sint64 field.
*/

/*!
    \typedef QtProtobuf::fixed32List

    A list of QtProtobuf::fixed32, the type of a repeated fixed32 field.
*/

/*!
    \typedef QtProtobuf::fixed64List

    A list of QtProtobuf::fixed64, the type of a repeated fixed64 field.
*/

/*!
    \typedef QtProtobuf::sfixed32List

    A list of QtProtobuf::sfixed32, the type of a repeated sfixed32 field.
*/

/*!
    \typedef QtProtobuf::sfixed64List

    A list of QtProtobuf::sfixed64, the type of a repeated sfixed64 field.
*/

/*!
    \typedef QtProtobuf::boolList

    A list of bool, the type of a repeated bool field.
*/

/*!
    \typedef QtProtobuf::floatList

    A list of float, the type of a repeated float field.
*/

/*!
    \typedef QtProtobuf::doubleList

    A list of double, the type of a repeated double field.
*/

/*!
    \enum QtProtobuf::WireTypes

    This enum describes how a field is encoded on the wire.

    \value Unknown Not a known encoding.
    \value Varint A variable-length integer.
    \value Fixed64 Eight bytes.
    \value LengthDelimited A length followed by that many bytes: strings, bytes, messages and packed lists.
    \value StartGroup The start of a group of proto2. Not supported.
    \value EndGroup The end of a group of proto2. Not supported.
    \value Fixed32 Four bytes.
*/

/*!
    \variable QtProtobuf::InvalidFieldNumber

    The field number 0, which no field has: the value of the \c UninitializedField of a oneof.
*/

/*!
    Registers the types of the generated messages. Nt needs no registration, so this does nothing;
    it is there for code written for Qt.
*/
void QtProtobuf::qRegisterProtobufTypes() {}

/*!
    \class QProtobufMessage
    \inmodule NtProtobuf
    \brief The QProtobufMessage class is the base of the message classes that ntprotobufgen generates.
    \since 6.0

    A generated message class has a getter and a setter for each field of the message. Write it
    with serialize() and read it with deserialize(), through a serializer:

    \code
    tutorial::Person person;
    person.setName(QStringLiteral("Ada"));
    person.setId(1815);

    QProtobufSerializer serializer;
    const QByteArray data = person.serialize(&serializer);

    tutorial::Person copy;
    if (!copy.deserialize(&serializer, data))
        qWarning() << serializer.lastErrorString();
    \endcode

    property() and setProperty() reach a field by the name of its getter.

    \differences
    The serializers call functions that ntprotobufgen generates rather than reading the fields
    through the meta-object system, so a serializer of your own cannot be written. property() and
    setProperty() handle the scalar, string, bytes and enum fields; messages, lists and maps are
    reached through the getters.
    \enddifferences

    \sa QProtobufSerializer, QProtobufJsonSerializer, {Using Nt Protobuf}
*/

/*!
    Constructs an empty message.
*/
QProtobufMessage::QProtobufMessage() = default;

/*!
    Constructs a copy of \a other.
*/
QProtobufMessage::QProtobufMessage(const QProtobufMessage &other) = default;

/*!
    Constructs a message by moving \a other.
*/
QProtobufMessage::QProtobufMessage(QProtobufMessage &&other) noexcept = default;

/*!
    Assigns \a other to this message.
*/
QProtobufMessage &QProtobufMessage::operator=(const QProtobufMessage &other) = default;

/*!
    Moves \a other into this message.
*/
QProtobufMessage &QProtobufMessage::operator=(QProtobufMessage &&other) noexcept = default;

/*!
    Destroys the message.
*/
QProtobufMessage::~QProtobufMessage() = default;

/*!
    Returns the value of the field whose getter is called \a propertyName, or an invalid QVariant
    if there is none or it is a message, list or map.
*/
QVariant QProtobufMessage::property(QAnyStringView propertyName) const { return ntProperty(propertyName); }

/*!
    Sets the field whose getter is called \a propertyName to \a value. Returns \c false if there is
    no such scalar, string, bytes or enum field, or \a value cannot be converted to its type.
*/
bool QProtobufMessage::setProperty(QAnyStringView propertyName, const QVariant &value) {
    return ntSetProperty(propertyName, value);
}

/*!
    Returns a new message of the type called \a messageType, with its package, such as
    \c tutorial.Person, or a null pointer if no generated class of that name is linked in.
*/
QProtobufMessagePointer QProtobufMessage::constructByName(const QString &messageType) {
    QtProtobufPrivate::Registry &r = QtProtobufPrivate::registry();
    const QtProtobufPrivate::QProtobufPropertyOrdering *ordering = nullptr;
    {
        const std::lock_guard<std::mutex> lock(r.mutex);
        ordering = r.messages.value(messageType, nullptr);
    }
    return QProtobufMessagePointer(ordering ? ordering->create() : nullptr);
}

/*!
    Returns the message written by \a serializer.

    \sa deserialize()
*/
QByteArray QProtobufMessage::serialize(QAbstractProtobufSerializer *serializer) const {
    return serializer ? serializer->serialize(this) : QByteArray();
}

/*!
    Replaces the message with the one that \a serializer reads from \a data. Returns \c false on an
    error, which the serializer's QAbstractProtobufSerializer::lastError() describes.

    \sa serialize()
*/
bool QProtobufMessage::deserialize(QAbstractProtobufSerializer *serializer, QByteArrayView data) {
    return serializer && serializer->deserialize(this, data);
}

/*!
    Returns what the class knows of itself: its full name and how to make one, or \c nullptr for a
    class that ntprotobufgen did not generate.
*/
const QtProtobufPrivate::QProtobufPropertyOrdering *QProtobufMessage::propertyOrdering() const { return nullptr; }

/*!
    Returns the numbers of the fields that the binary data had but the class does not know, which
    are kept and written again by QProtobufSerializer.

    \sa unknownFieldData()
*/
QList<qint32> QProtobufMessage::unknownFieldNumbers() const {
    QList<qint32> numbers = m_unknown.keys();
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}

/*!
    Returns the encoded fields, each with its tag, that the binary data had under the number
    \a field and the class does not know.
*/
QList<QByteArray> QProtobufMessage::unknownFieldData(qint32 field) const { return m_unknown.value(field); }

/*!
    \internal
*/
void QProtobufMessage::ntClear() { m_unknown.clear(); }

/*!
    \internal
*/
void QProtobufMessage::ntEncode(QtProtobufPrivate::Writer &writer) const { (void)writer; }

/*!
    \internal
*/
bool QProtobufMessage::ntDecode(QtProtobufPrivate::Reader &reader) {
    int field = 0;
    QtProtobuf::WireTypes wire = QtProtobuf::WireTypes::Unknown;
    while (reader.next(&field, &wire))
        reader.unknown(this);
    return !reader.failed();
}

/*!
    \internal
*/
void QProtobufMessage::ntToJson(QJsonObject &object) const { (void)object; }

/*!
    \internal
*/
bool QProtobufMessage::ntFromJson(const QJsonObject &object, QString *error) {
    (void)object;
    (void)error;
    return true;
}

/*!
    \internal
*/
QVariant QProtobufMessage::ntProperty(QAnyStringView name) const {
    (void)name;
    return QVariant();
}

/*!
    \internal
*/
bool QProtobufMessage::ntSetProperty(QAnyStringView name, const QVariant &value) {
    (void)name;
    (void)value;
    return false;
}

/*!
    \class QProtobufMessageDeleter
    \inmodule NtProtobuf
    \brief The QProtobufMessageDeleter class deletes the message that a QProtobufMessagePointer holds.
    \since 6.0

    \differences
    None.
    \enddifferences
*/

/*!
    Deletes \a message.
*/
void QProtobufMessageDeleter::operator()(QProtobufMessage *message) const { delete message; }
