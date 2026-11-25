// MIT License - see LICENSE
//
// Nt Protobuf: the classes that ntprotobufgen generated from tests/protobuf, written and read with QProtobufSerializer
// (checked byte for byte against the wire format, and against protoc when it is installed) and
// QProtobufJsonSerializer.
#include <QtCore>
#include <QtProtobuf>

#include "nttest/common.qpb.h"
#include "nttest/everything.qpb.h"
#include "nttest/legacy.qpb.h"

#include <cmath>
#include <cstdio>
#include <limits>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

QByteArray hex(const char *text) { return QByteArray::fromHex(QByteArray(text)); }

template <typename T> T roundTrip(const T &message, QAbstractProtobufSerializer *serializer) {
    T copy;
    const QByteArray data = message.serialize(serializer);
    if (!copy.deserialize(serializer, data))
        std::printf("  deserialize failed: %s\n", qPrintable(serializer->lastErrorString()));
    return copy;
}

void testWireFormat() {
    QProtobufSerializer serializer;
    nttest::Scalars s;
    CHECK(s.serialize(&serializer).isEmpty()); // defaults are left out

    s.setAnInt32(150);
    CHECK(s.serialize(&serializer) == hex("189601"));
    s.setAnInt32(-1); // a negative int32 is ten bytes
    CHECK(s.serialize(&serializer) == hex("18ffffffffffffffffff01"));
    s = nttest::Scalars();
    s.setASint32(-1); // zig-zag
    CHECK(s.serialize(&serializer) == hex("3801"));
    s = nttest::Scalars();
    s.setASint64(-2);
    CHECK(s.serialize(&serializer) == hex("4003"));
    s = nttest::Scalars();
    s.setAFixed32(1);
    CHECK(s.serialize(&serializer) == hex("4d01000000"));
    s = nttest::Scalars();
    s.setASfixed64(-1);
    CHECK(s.serialize(&serializer) == hex("61ffffffffffffffff"));
    s = nttest::Scalars();
    s.setAFloat(1.0f);
    CHECK(s.serialize(&serializer) == hex("150000803f"));
    s = nttest::Scalars();
    s.setADouble(-0.0); // not the default
    CHECK(s.serialize(&serializer) == hex("090000000000000080"));
    s = nttest::Scalars();
    s.setAString(QStringLiteral("hi"));
    s.setABool(true);
    CHECK(s.serialize(&serializer) == hex("680172026869"));
    s.setAUint64(std::numeric_limits<quint64>::max());
    CHECK(s.serialize(&serializer) == hex("30ffffffffffffffffff01680172026869"));

    // Every scalar survives a round trip.
    nttest::Scalars all;
    all.setADouble(3.25);
    all.setAFloat(-1.5f);
    all.setAnInt32(std::numeric_limits<qint32>::min());
    all.setAnInt64(std::numeric_limits<qint64>::min());
    all.setAUint32(std::numeric_limits<quint32>::max());
    all.setAUint64(std::numeric_limits<quint64>::max());
    all.setASint32(std::numeric_limits<qint32>::min());
    all.setASint64(std::numeric_limits<qint64>::max());
    all.setAFixed32(0xdeadbeef);
    all.setAFixed64(0x0123456789abcdefULL);
    all.setASfixed32(-123456);
    all.setASfixed64(-1234567890123LL);
    all.setABool(true);
    all.setAString(QString::fromUtf8("h\xc3\xa9llo"));
    all.setSomeBytes(QByteArray("\x00\x01\xff", 3));
    const nttest::Scalars copy = roundTrip(all, &serializer);
    CHECK(copy == all);
    CHECK(copy.aSint32() == std::numeric_limits<qint32>::min());
    CHECK(copy.aUint64() == std::numeric_limits<quint64>::max());
    CHECK(copy.someBytes() == QByteArray("\x00\x01\xff", 3));
    CHECK(copy.aString() == QString::fromUtf8("h\xc3\xa9llo"));
}

nttest::Person makePerson() {
    nttest::Person p;
    p.setName(QStringLiteral("Ada"));
    p.setId(1815);
    nttest::Person::PhoneNumber home;
    home.setNumber(QStringLiteral("555-0100"));
    home.setType(nttest::Person::PhoneType::HOME);
    p.mutPhones().append(home);
    nttest::Person_QtProtobufNested::PhoneNumber work;
    work.setNumber(QStringLiteral("555-0199"));
    work.setType(nttest::Person::PhoneType::WORK);
    p.mutPhones().append(work);
    p.mutScores().insert(QStringLiteral("chess"), 1200);
    p.mutScores().insert(QStringLiteral("go"), -3);
    p.setFax(QStringLiteral("555-0111"));
    p.setBig(-9000000000LL);
    p.setBlob(QByteArray("blob"));
    p.setMood(nttest::Mood::SAD);
    nttest::common::Color color;
    color.setRgb(0x336699);
    color.setShade(nttest::common::Shade::DARK);
    p.setFavourite(color);
    p.setLucky({1, 2, 3});
    p.setUnlucky({4, 5});
    p.setTags({QStringLiteral("a"), QStringLiteral("b")});
    p.setMoods({nttest::Mood::HAPPY, nttest::Mood::SAD});
    p.mutBySlot().insert(7, home);
    p.mutFlags().insert(true, QStringLiteral("yes"));
    p.setWeights({0.5, 1.5});
    p.setShade(nttest::common::Shade::LIGHT);
    return p;
}

void testMessages() {
    QProtobufSerializer serializer;
    const nttest::Person p = makePerson();
    const nttest::Person copy = roundTrip(p, &serializer);
    CHECK(copy == p);
    CHECK(copy.phones().size() == 2 && copy.phones().at(1).number() == QStringLiteral("555-0199"));
    CHECK(copy.scores().value(QStringLiteral("go")) == -3);
    CHECK(copy.favourite().rgb() == 0x336699);
    CHECK(copy.bySlot().value(7).type() == nttest::Person::PhoneType::HOME);
    CHECK(copy.moods() == p.moods());
    CHECK(copy.mood() == nttest::Mood::SAD);

    // Packed and unpacked lists.
    nttest::Person lists;
    lists.setLucky({1, 2, 3});
    CHECK(lists.serialize(&serializer) == hex("6a03010203"));
    lists = nttest::Person();
    lists.setUnlucky({1, 2});
    CHECK(lists.serialize(&serializer) == hex("70017002"));
    // Either form is read for either field.
    nttest::Person read;
    CHECK(read.deserialize(&serializer, hex("680168026a020304")));
    CHECK((read.lucky() == QtProtobuf::int32List{1, 2, 3, 4}));
    CHECK(read.deserialize(&serializer, hex("72020506")));
    CHECK((read.unlucky() == QtProtobuf::int32List{5, 6}));

    // Presence: an optional field that is set is written even when it holds its default.
    nttest::Person optional;
    CHECK(!optional.hasEmail() && !optional.hasMaybe());
    optional.setMaybe(0);
    CHECK(optional.hasMaybe());
    CHECK(optional.serialize(&serializer) == hex("980100"));
    optional.clearMaybe();
    CHECK(!optional.hasMaybe() && optional.serialize(&serializer).isEmpty());
    optional.setEmail(QString());
    CHECK(optional.serialize(&serializer) == hex("1a00"));
    // A message field has presence too.
    nttest::Person withColor;
    CHECK(!withColor.hasFavourite());
    withColor.mutFavourite();
    CHECK(withColor.hasFavourite() && withColor.serialize(&serializer) == hex("6200"));

    // A oneof holds one field at a time.
    nttest::Person oneof;
    CHECK(oneof.contactField() == nttest::Person::ContactFields::UninitializedField);
    oneof.setPhone(QStringLiteral("1"));
    CHECK(oneof.hasPhone() && oneof.contactField() == nttest::Person::ContactFields::Phone);
    oneof.setFax(QStringLiteral("2"));
    CHECK(!oneof.hasPhone() && oneof.hasFax() && oneof.phone().isEmpty());
    oneof.mutPager().setNumber(QStringLiteral("3"));
    CHECK(oneof.contactField() == nttest::Person::ContactFields::Pager && oneof.fax().isEmpty());
    const nttest::Person oneofCopy = roundTrip(oneof, &serializer);
    CHECK(oneofCopy.hasPager() && oneofCopy.pager().number() == QStringLiteral("3"));
    oneof.clearContact();
    CHECK(oneof.contactField() == nttest::Person::ContactFields::UninitializedField);
    // An empty string in a oneof is still written.
    oneof.setPhone(QString());
    CHECK(oneof.serialize(&serializer) == hex("3200"));

    // A message that holds itself.
    nttest::Node root;
    root.setLabel(QStringLiteral("root"));
    nttest::Node child;
    child.setLabel(QStringLiteral("child"));
    child.mutChildren().append(nttest::Node());
    root.mutChildren().append(child);
    root.mutParent().setLabel(QStringLiteral("up"));
    const nttest::Node rootCopy = roundTrip(root, &serializer);
    CHECK(rootCopy == root && rootCopy.children().at(0).children().size() == 1);
    CHECK(rootCopy.parent().label() == QStringLiteral("up"));

    // Copies share their data until one changes.
    nttest::Person a = makePerson();
    nttest::Person b = a;
    b.setName(QStringLiteral("Grace"));
    CHECK(a.name() == QStringLiteral("Ada") && b.name() == QStringLiteral("Grace") && a != b);

    // Field numbers.
    CHECK(nttest::Person::BigProtoFieldNumber == 9);
    CHECK(nttest::Person::BySlotProtoFieldNumber == 17);
}

void testUnknownFields() {
    QProtobufSerializer serializer;
    const QByteArray data = makePerson().serialize(&serializer);
    // A message that knows none of the fields keeps them all and writes them back as they were.
    nttest::Empty empty;
    CHECK(empty.deserialize(&serializer, data));
    CHECK(empty.unknownFieldNumbers().contains(1) && empty.unknownFieldNumbers().contains(17));
    CHECK(empty.unknownFieldData(4).size() == 2); // the two phone numbers
    CHECK(empty.serialize(&serializer) == data);
    nttest::Person back;
    CHECK(back.deserialize(&serializer, empty.serialize(&serializer)) && back == makePerson());
    // Unless told not to keep them.
    QProtobufSerializer forgetful;
    forgetful.shouldPreserveUnknownFields(false);
    CHECK(empty.deserialize(&forgetful, data));
    CHECK(empty.unknownFieldNumbers().isEmpty() && empty.serialize(&forgetful).isEmpty());
}

void testErrors() {
    QProtobufSerializer serializer;
    nttest::Person p;
    CHECK(!p.deserialize(&serializer, hex("0a05616263"))); // a string longer than the data
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::UnexpectedEndOfStream);
    CHECK(!serializer.lastErrorString().isEmpty());
    CHECK(!p.deserialize(&serializer, hex("1096"))); // a varint cut short
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::UnexpectedEndOfStream);
    CHECK(!p.deserialize(&serializer, hex("0b0c"))); // a group
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::InvalidHeader);
    CHECK(!p.deserialize(&serializer, hex("0001"))); // field number 0
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::InvalidHeader);
    CHECK(!p.deserialize(&serializer, hex("0d01000000"))); // name (a string) as fixed32
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::InvalidHeader);
    CHECK(!p.deserialize(&serializer, hex("10ffffffffffffffffffff01"))); // an eleven-byte varint
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::InvalidFormat);
    // A good message clears the error.
    CHECK(p.deserialize(&serializer, hex("1001")) && p.id() == 1);
    CHECK(serializer.lastError() == QAbstractProtobufSerializer::Error::None);
    // deserialize() replaces what the message held.
    CHECK(p.deserialize(&serializer, hex("0a017a")) && p.id() == 0 && p.name() == QStringLiteral("z"));
}

void testJson() {
    QProtobufJsonSerializer json;
    nttest::Person p = makePerson();
    const QByteArray text = p.serialize(&json);
    const QJsonObject o = QJsonDocument::fromJson(text).object();
    CHECK(o.value(QStringLiteral("name")).toString() == QStringLiteral("Ada"));
    CHECK(o.value(QStringLiteral("id")).toInt() == 1815);
    CHECK(o.value(QStringLiteral("hugeNumber")).toString() ==
          QStringLiteral("-9000000000")); // json_name, int64 as text
    CHECK(o.value(QStringLiteral("mood")).toString() == QStringLiteral("SAD"));
    CHECK(o.value(QStringLiteral("blob")).toString() == QStringLiteral("YmxvYg=="));
    CHECK(o.value(QStringLiteral("phones")).toArray().size() == 2);
    CHECK(o.value(QStringLiteral("phones")).toArray().at(0).toObject().value(QStringLiteral("type")).toString() ==
          QStringLiteral("HOME"));
    CHECK(o.value(QStringLiteral("scores")).toObject().value(QStringLiteral("go")).toInt() == -3);
    CHECK(o.value(QStringLiteral("bySlot")).toObject().contains(QStringLiteral("7")));
    CHECK(o.value(QStringLiteral("flags")).toObject().value(QStringLiteral("true")).toString() ==
          QStringLiteral("yes"));
    CHECK(o.value(QStringLiteral("fax")).toString() == QStringLiteral("555-0111"));
    CHECK(!o.contains(QStringLiteral("phone")) && !o.contains(QStringLiteral("email")));
    CHECK(o.value(QStringLiteral("favourite")).toObject().value(QStringLiteral("shade")).toString() ==
          QStringLiteral("DARK"));
    const nttest::Person back = roundTrip(p, &json);
    CHECK(back == p);

    // Reading: proto names, numbers as strings, enum numbers, null and unknown names.
    nttest::Person read;
    CHECK(read.deserialize(&json, R"({"name":"Bo","id":"42","big":"7","mood":-2,"email":null,"whatever":[1,2],
                                      "by_slot":{"3":{"number":"x"}},"lucky":[1,"2"],"blob":"YmxvYg"})"));
    CHECK(read.name() == QStringLiteral("Bo") && read.id() == 42 && read.big() == 7);
    CHECK(read.mood() == nttest::Mood::SAD && !read.hasEmail());
    CHECK(read.bySlot().value(3).number() == QStringLiteral("x"));
    CHECK((read.lucky() == QtProtobuf::int32List{1, 2}));
    CHECK(read.blob() == QByteArray("blob"));

    // Floats.
    nttest::Scalars s;
    s.setADouble(std::numeric_limits<double>::quiet_NaN());
    s.setAFloat(-std::numeric_limits<float>::infinity());
    s.setAUint64(18446744073709551615ULL);
    const QJsonObject so = QJsonDocument::fromJson(s.serialize(&json)).object();
    CHECK(so.value(QStringLiteral("aDouble")).toString() == QStringLiteral("NaN"));
    CHECK(so.value(QStringLiteral("aFloat")).toString() == QStringLiteral("-Infinity"));
    CHECK(so.value(QStringLiteral("aUint64")).toString() == QStringLiteral("18446744073709551615"));
    nttest::Scalars sBack;
    CHECK(sBack.deserialize(&json, s.serialize(&json)));
    CHECK(std::isnan(sBack.aDouble()) && std::isinf(sBack.aFloat()) && sBack.aUint64() == s.aUint64());

    // Errors.
    CHECK(!read.deserialize(&json, "{not json"));
    CHECK(json.lastError() == QAbstractProtobufSerializer::Error::InvalidFormat);
    CHECK(!read.deserialize(&json, R"({"id":"twelve"})"));
    CHECK(json.lastError() == QAbstractProtobufSerializer::Error::InvalidFormat);
    CHECK(!read.deserialize(&json, R"({"id":3000000000})")); // out of the range of int32
    CHECK(!read.deserialize(&json, R"({"mood":"GRUMPY"})"));
    CHECK(!read.deserialize(&json, R"({"phones":{}})"));
    CHECK(read.deserialize(&json, "{}") && json.lastError() == QAbstractProtobufSerializer::Error::None);
}

void testProto2() {
    QProtobufSerializer serializer;
    nttest::legacy::Settings s;
    CHECK(s.title() == QStringLiteral("untitled") && s.retries() == 3 && s.ratio() == 0.5);
    CHECK(s.level() == nttest::legacy::Settings::Level::HIGH);
    CHECK(!s.hasTitle() && !s.hasVersion());
    CHECK(s.serialize(&serializer).isEmpty());
    s.setVersion(0); // set, so written although zero
    CHECK(s.serialize(&serializer) == hex("0800"));
    s.setCodes({1, 2});
    s.setPackedCodes({3, 4});
    CHECK(s.serialize(&serializer) == hex("0800300130023a020304"));
    s.setTitle(QStringLiteral("x"));
    s.clearTitle();
    CHECK(s.title() == QStringLiteral("untitled") && !s.hasTitle());
    nttest::legacy::Settings back = roundTrip(s, &serializer);
    CHECK(back == s && back.hasVersion() && back.version() == 0);
}

void testReflection() {
    nttest::Person p = makePerson();
    CHECK(p.property("name").toString() == QStringLiteral("Ada"));
    CHECK(p.property("id").toInt() == 1815);
    CHECK(p.property("big").toLongLong() == -9000000000LL);
    CHECK(p.property("mood").toInt() == -2);
    CHECK(!p.property("phones").isValid() && !p.property("nothing").isValid());
    CHECK(p.setProperty("name", QStringLiteral("Bea")) && p.name() == QStringLiteral("Bea"));
    CHECK(p.setProperty("id", 7) && p.id() == 7);
    CHECK(!p.setProperty("nothing", 1));
    CHECK(!p.setProperty("id", QVariant()));

    QProtobufMessagePointer made = QProtobufMessage::constructByName(QStringLiteral("nttest.Person"));
    CHECK(made && made->propertyOrdering());
    CHECK(made && made->propertyOrdering()->getMessageFullName() == QAnyStringView("nttest.Person"));
    CHECK(QProtobufMessage::constructByName(QStringLiteral("nttest.Person.PhoneNumber")) != nullptr);
    CHECK(QProtobufMessage::constructByName(QStringLiteral("nttest.Nobody")) == nullptr);
    if (made) {
        QProtobufSerializer serializer;
        CHECK(made->deserialize(&serializer, makePerson().serialize(&serializer)));
        CHECK(made->property("name").toString() == QStringLiteral("Ada"));
    }
}

void testProtoc() {
#ifdef NT_PROTOC
    // protoc decodes what Nt writes, and Nt reads what protoc encodes.
    QProtobufSerializer serializer;
    QProcess decode;
    decode.start(QStringLiteral(NT_PROTOC),
                 {QStringLiteral("--decode=nttest.Person"), QStringLiteral("-I") + QStringLiteral(NT_PROTO_DIR),
                  QStringLiteral(NT_PROTO_DIR "/nttest/everything.proto")});
    CHECK(decode.waitForStarted());
    decode.write(makePerson().serialize(&serializer));
    decode.closeWriteChannel();
    CHECK(decode.waitForFinished());
    const QByteArray text = decode.readAllStandardOutput();
    CHECK(decode.exitCode() == 0);
    CHECK(text.contains("name: \"Ada\""));
    CHECK(text.contains("big: -9000000000"));
    CHECK(text.contains("mood: SAD"));
    CHECK(text.contains("number: \"555-0199\""));
    CHECK(text.contains("lucky: 1") && text.contains("unlucky: 5"));
    CHECK(text.contains("fax: \"555-0111\""));
    if (decode.exitCode() != 0 || !text.contains("name: \"Ada\""))
        std::printf("protoc said:\n%s\n%s\n", text.constData(), decode.readAllStandardError().constData());

    QProcess encode;
    encode.start(QStringLiteral(NT_PROTOC),
                 {QStringLiteral("--encode=nttest.Person"), QStringLiteral("-I") + QStringLiteral(NT_PROTO_DIR),
                  QStringLiteral(NT_PROTO_DIR "/nttest/everything.proto")});
    CHECK(encode.waitForStarted());
    encode.write("name: \"Lin\" id: -5 phones { number: \"1\" type: WORK } scores { key: \"k\" value: 9 } "
                 "pager { number: \"p\" } big: 123456789012 lucky: [7, 8] moods: [HAPPY, SAD] "
                 "by_slot { key: 2 value { number: \"s\" } } maybe: 0 favourite { rgb: 255 }\n");
    encode.closeWriteChannel();
    CHECK(encode.waitForFinished());
    nttest::Person p;
    CHECK(encode.exitCode() == 0);
    CHECK(p.deserialize(&serializer, encode.readAllStandardOutput()));
    CHECK(p.name() == QStringLiteral("Lin") && p.id() == -5);
    CHECK(p.phones().size() == 1 && p.phones().at(0).type() == nttest::Person::PhoneType::WORK);
    CHECK(p.scores().value(QStringLiteral("k")) == 9);
    CHECK(p.hasPager() && p.pager().number() == QStringLiteral("p"));
    CHECK(p.big() == 123456789012LL);
    CHECK((p.lucky() == QtProtobuf::int32List{7, 8}));
    CHECK((p.moods() == nttest::MoodRepeated{nttest::Mood::HAPPY, nttest::Mood::SAD}));
    CHECK(p.bySlot().value(2).number() == QStringLiteral("s"));
    CHECK(p.hasMaybe() && p.maybe() == 0);
    CHECK(p.favourite().rgb() == 255);
    std::printf("checked against %s\n", NT_PROTOC);
#else
    std::printf("protoc not found: not checked against it\n");
#endif
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testWireFormat();
    testMessages();
    testUnknownFields();
    testErrors();
    testJson();
    testProto2();
    testReflection();
    testProtoc();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all protobuf tests passed\n");
    return 0;
}
