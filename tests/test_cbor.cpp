// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QCborStreamReader>
#include <QtCore/QCborStreamWriter>
#include <QtCore/QCborValue>

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace Qt::StringLiterals;
using R = QCborStreamReader;
using V = QCborValue;

static QByteArray hex(const char *text) { return QByteArray::fromHex(QByteArray(text)); }
static QByteArray encode(const QCborValue &v, V::EncodingOptions opt = V::NoTransformation) { return v.toCbor(opt); }
static QString diag(const QByteArray &cbor) { return V::fromCbor(cbor).toDiagnosticNotation(); }

// The examples of RFC 8949, appendix A: the encoding and what it decodes to.
static void testRfcExamples() {
    const struct {
        const char *hex;
        const char *diagnostic;
    } examples[] = {
        {"00", "0"},
        {"01", "1"},
        {"0a", "10"},
        {"17", "23"},
        {"1818", "24"},
        {"1819", "25"},
        {"1864", "100"},
        {"1903e8", "1000"},
        {"1a000f4240", "1000000"},
        {"1b000000e8d4a51000", "1000000000000"},
        {"20", "-1"},
        {"29", "-10"},
        {"3863", "-100"},
        {"3903e7", "-1000"},
        {"f90000", "0.0"},
        {"f98000", "-0.0"},
        {"f93c00", "1.0"},
        {"fb3ff199999999999a", "1.1"},
        {"f93e00", "1.5"},
        {"f97bff", "65504.0"},
        {"fa47c35000", "100000.0"},
        {"fa7f7fffff", "3.4028234663852886e+38"},
        {"fb7e37e43c8800759c", "1e+300"},
        {"f90001", "5.9604644775390625e-08"},
        {"f90400", "6.103515625e-05"},
        {"f9c400", "-4.0"},
        {"fbc010666666666666", "-4.1"},
        {"f97c00", "Infinity"},
        {"f97e00", "NaN"},
        {"f9fc00", "-Infinity"},
        {"fa7f800000", "Infinity"},
        {"fb7ff8000000000000", "NaN"},
        {"f4", "false"},
        {"f5", "true"},
        {"f6", "null"},
        {"f7", "undefined"},
        {"f0", "simple(16)"},
        {"f8ff", "simple(255)"},
        {"c074323031332d30332d32315432303a30343a30305a", "0(\"2013-03-21T20:04:00Z\")"},
        {"c11a514b67b0", "1(1363896240)"},
        {"c1fb41d452d9ec200000", "1(1363896240.5)"},
        {"d74401020304", "23(h'01020304')"},
        {"d818456449455446", "24(h'6449455446')"},
        {"d82076687474703a2f2f7777772e6578616d706c652e636f6d", "32(\"http://www.example.com\")"},
        {"40", "h''"},
        {"4401020304", "h'01020304'"},
        {"60", "\"\""},
        {"6161", "\"a\""},
        {"6449455446", "\"IETF\""},
        {"62225c", "\"\\\"\\\\\""},
        {"62c3bc", "\"\xc3\xbc\""},
        {"63e6b0b4", "\"\xe6\xb0\xb4\""},
        {"64f0908591", "\"\xf0\x90\x85\x91\""},
        {"80", "[]"},
        {"83010203", "[1, 2, 3]"},
        {"8301820203820405", "[1, [2, 3], [4, 5]]"},
        {"98190102030405060708090a0b0c0d0e0f101112131415161718181819",
         "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]"},
        {"a0", "{}"},
        {"a201020304", "{1: 2, 3: 4}"},
        {"a26161016162820203", "{\"a\": 1, \"b\": [2, 3]}"},
        {"826161a161626163", "[\"a\", {\"b\": \"c\"}]"},
        {"a56161614161626142616361436164614461656145",
         "{\"a\": \"A\", \"b\": \"B\", \"c\": \"C\", \"d\": \"D\", \"e\": \"E\"}"},
        {"5f42010243030405ff", "h'0102030405'"},
        {"7f657374726561646d696e67ff", "\"streaming\""},
        {"9fff", "[]"},
        {"9f018202039f0405ffff", "[1, [2, 3], [4, 5]]"},
        {"9f01820203820405ff", "[1, [2, 3], [4, 5]]"},
        {"83018202039f0405ff", "[1, [2, 3], [4, 5]]"},
        {"83019f0203ff820405", "[1, [2, 3], [4, 5]]"},
        {"9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff",
         "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]"},
        {"bf61610161629f0203ffff", "{\"a\": 1, \"b\": [2, 3]}"},
        {"826161bf61626163ff", "[\"a\", {\"b\": \"c\"}]"},
        {"bf6346756ef563416d7421ff", "{\"Fun\": true, \"Amt\": -2}"},
    };
    for (const auto &example : examples) {
        QCborParserError error;
        const QCborValue v = V::fromCbor(hex(example.hex), &error);
        const QString got = v.toDiagnosticNotation();
        const bool ok = error.error == QCborError::NoError && got == QString::fromUtf8(example.diagnostic);
        CHECK(ok);
        if (!ok)
            std::printf("  %s: %s (%s)\n", example.hex, got.toUtf8().constData(),
                        error.errorString().toUtf8().constData());
    }

    // What encodes with the shortest head, encodes as the RFC shows
    for (const char *h : {"00",
                          "17",
                          "1818",
                          "1903e8",
                          "1a000f4240",
                          "1b000000e8d4a51000",
                          "20",
                          "3903e7",
                          "f4",
                          "f5",
                          "f6",
                          "f7",
                          "f0",
                          "f8ff",
                          "40",
                          "4401020304",
                          "6449455446",
                          "83010203",
                          "8301820203820405",
                          "a26161016162820203",
                          "d74401020304",
                          "fb3ff199999999999a"}) {
        const QByteArray bytes = hex(h);
        const QByteArray again = V::fromCbor(bytes).toCbor();
        CHECK(again == bytes);
        if (again != bytes)
            std::printf("  %s re-encodes as %s\n", h, again.toHex().constData());
    }
}

static void testValues() {
    const QCborValue undefinedValue;
    CHECK(undefinedValue.isUndefined() && undefinedValue.type() == V::Undefined && undefinedValue.isSimpleType());
    CHECK(undefinedValue.toSimpleType() == QCborSimpleType::Undefined && !undefinedValue.isNull());

    CHECK(V(nullptr).isNull() && V(true).isTrue() && V(false).isFalse() && V(true).isBool() &&
          V(false).toBool(true) == false);
    CHECK(V(true).toBool() && V(1).toBool(true) == true && V(1).toBool() == false);
    CHECK(V(QCborSimpleType::Null).isNull() && V(QCborSimpleType(5)).isSimpleType() &&
          V(QCborSimpleType(5)).toSimpleType() == QCborSimpleType(5));
    CHECK(V(QCborSimpleType(5)).isSimpleType(QCborSimpleType(5)) &&
          !V(QCborSimpleType(5)).isSimpleType(QCborSimpleType(6)));

    // Integers
    CHECK(V(42).isInteger() && V(42).toInteger() == 42 && V(42u).toInteger() == 42 && V(qint64(-7)).toInteger() == -7);
    CHECK(V(42).toDouble() == 42.0 && V(42).toInteger(5) == 42 && V(1.5).toInteger(5) == 5);
    CHECK(V(std::numeric_limits<qint64>::min()).toInteger() == std::numeric_limits<qint64>::min());
    CHECK(V(42LL).toInteger() == 42);
    CHECK(V(1.5).isDouble() && V(1.5).toDouble() == 1.5 && V(u"x"_s).toDouble(9) == 9);

    // Strings and byte arrays
    CHECK(V(u"text"_s).isString() && V(u"text"_s).toString() == u"text"_s &&
          V("utf8 \xc3\xa4").toString() == QString::fromUtf8("utf8 \xc3\xa4"));
    CHECK(V(QStringView(u"view")).toString() == u"view"_s && V(QLatin1StringView("latin")).toString() == u"latin"_s);
    CHECK(V(QByteArray("\x01\x02", 2)).isByteArray() &&
          V(QByteArray("\x01\x02", 2)).toByteArray() == QByteArray("\x01\x02", 2));
    CHECK(V(1).toString(u"default"_s) == u"default"_s && V(1).toByteArray("d") == QByteArray("d"));
    CHECK(V(QString()).isString() && V(QString()).toString().isEmpty());

    // Types made from Type
    CHECK(V(V::Integer).toInteger(-1) == 0 && V(V::String).isString() && V(V::Array).isArray() && V(V::Map).isMap());
    CHECK(V(V::Double).isDouble() && V(V::Null).isNull() && V(V::True).isTrue() && V(V::Invalid).isInvalid());
    CHECK(V(V::Tag).isTag() && V(V::Tag).tag() == QCborTag(0) && V(V::SimpleType).isSimpleType());

    // Tags and the extended types
    const V tagged(QCborTag(1234), V(u"inside"_s));
    CHECK(tagged.isTag() && tagged.type() == V::Tag && tagged.tag() == QCborTag(1234) &&
          tagged.taggedValue() == V(u"inside"_s));
    CHECK(V(1).tag() == QCborTag(~0ULL) && V(1).taggedValue(V(7)) == V(7));
    const QDateTime when(QDate(2026, 11, 12), QTime(10, 20, 30, 456), Qt::UTC);
    const V date(when);
    CHECK(date.isDateTime() && date.isTag() && date.tag() == QCborTag(0) && date.toDateTime() == when);
    CHECK(date.taggedValue().isString() && date.taggedValue().toString().startsWith(u"2026-11-12T10:20:30.456"_s));
    CHECK(V(QCborKnownTags::DateTimeString, V(u"not a date"_s)).type() == V::Tag);
    CHECK(V(QCborKnownTags::DateTimeString, V(u"2013-03-21T20:04:00Z"_s)).isDateTime());
    const QUrl url(u"https://example.org/a?b=c"_s);
    CHECK(V(url).isUrl() && V(url).toUrl() == url && V(url).tag() == QCborTag(32));
    const QRegularExpression rx(u"^a+b$"_s);
    CHECK(V(rx).isRegularExpression() && V(rx).toRegularExpression().pattern() == rx.pattern());
    const QUuid uuid = QUuid::fromString(u"{67c8770b-44f1-410a-ab9a-f9b5446f13ee}"_s);
    CHECK(V(uuid).isUuid() && V(uuid).toUuid() == uuid && V(uuid).taggedValue().toByteArray().size() == 16);
    CHECK(V(QCborKnownTags::Uuid, V(QByteArray("short"))).type() == V::Tag);
    CHECK(V(1).toDateTime(when) == when && V(1).toUrl(url) == url && V(1).toUuid(uuid) == uuid);

    // Copies share until they change
    V a(u"shared"_s);
    V b = a;
    CHECK(a == b);
    b = V(u"changed"_s);
    CHECK(a.toString() == u"shared"_s && b.toString() == u"changed"_s);
    V moved = std::move(b);
    CHECK(moved.toString() == u"changed"_s && b.isUndefined()); // NOLINT(bugprone-use-after-move): moved-from state
    a.swap(moved);
    CHECK(a.toString() == u"changed"_s && moved.toString() == u"shared"_s);
}

static void testCompare() {
    CHECK(V(1) == V(1) && V(1) != V(2) && V(1) < V(2) && V(2) > V(1) && V(1) <= V(1) && V(1) >= V(1));
    CHECK(V(1) != V(1.0)); // different types
    CHECK(V(-5) < V(3) && V(1.5) < V(2.5) && V(qQNaN()) == V(qQNaN()) && V(1e300) < V(qQNaN()));
    CHECK(V(u"b"_s) < V(u"aa"_s)); // shorter first
    CHECK(V(u"a"_s) < V(u"b"_s) && V(QByteArray("ab")) < V(QByteArray("abc")));
    CHECK(V(1) < V(QByteArray()) && V(QByteArray()) < V(u""_s) && V(u""_s) < V(QCborArray()) &&
          V(QCborArray()) < V(QCborMap()));
    CHECK(V(QCborMap()) < V(QCborTag(0), V()) && V(QCborTag(0), V()) < V(false) && V(false) < V(true) &&
          V(nullptr) == V(nullptr));
    CHECK(V(QCborTag(1), V(1)) < V(QCborTag(2), V(0)) && V(QCborTag(1), V(1)) < V(QCborTag(1), V(2)));
    CHECK((QCborArray{1, 2} < QCborArray{1, 3}) && (QCborArray{5} < QCborArray{1, 1}) &&
          (QCborArray{1, 2} == QCborArray{1, 2}));
    CHECK((QCborMap{{1, 2}} == QCborMap{{1, 2}}) && (QCborMap{{1, 2}} != QCborMap{{1, 3}}) &&
          (QCborMap{{1, 2}} < QCborMap{{1, 3}}));
    CHECK(V(QCborArray{1, u"x"_s}) == V(QCborArray{1, u"x"_s}));
}

static void testArray() {
    QCborArray array;
    CHECK(array.isEmpty() && array.empty() && array.size() == 0 && array.at(0).isUndefined() &&
          std::as_const(array).first().isUndefined());
    array.append(1);
    array << u"two"_s << 3.0;
    array.prepend(true);
    CHECK(array.size() == 4 && array.at(0).isTrue() && array[1] == V(1) && array.last() == V(3.0) &&
          array.first() == V(true));
    CHECK(array.contains(V(u"two"_s)) && !array.contains(V(2)));
    array.insert(2, V(nullptr));
    CHECK(array.at(2).isNull() && array.size() == 5);
    array.insert(-1, V(9));
    CHECK(array.last() == V(9));
    array.insert(8, V(10)); // beyond the end: grows with undefined
    CHECK(array.size() == 9 && array.at(6).isUndefined() && array.at(7).isUndefined() && array.at(8) == V(10));
    CHECK(array.takeAt(8) == V(10) && array.takeLast().isUndefined() && array.takeFirst().isUndefined() == false);
    array.removeAt(100); // nothing
    CHECK(array.takeAt(100).isUndefined());

    // References change the array in place
    QCborArray changing{1, 2, 3};
    changing[1] = u"two"_s;
    changing.first() = 0;
    changing.last() = QCborArray{4};
    CHECK(changing == (QCborArray{0, u"two"_s, QCborArray{4}}));
    changing[5] = 6; // grows
    CHECK(changing.size() == 6 && changing.at(3).isUndefined() && changing.at(5) == V(6));
    QCborValueRef ref = changing[0];
    CHECK(ref.isInteger() && ref.toInteger() == 0 && ref == V(0) && ref.type() == V::Integer);
    ref = changing[1]; // assigns the value, not the reference
    CHECK(changing.at(0) == V(u"two"_s) && changing.at(1) == V(u"two"_s));
    const QCborValue concrete = changing[2];
    CHECK(concrete.isArray() && concrete.toArray().first() == V(4));

    // Iterators
    QCborArray numbers{1, 2, 3, 4};
    qint64 sum = 0;
    for (const QCborValue &v : numbers)
        sum += v.toInteger();
    CHECK(sum == 10);
    for (auto it = numbers.begin(); it != numbers.end(); ++it)
        *it = it->toInteger() * 10;
    CHECK(numbers == (QCborArray{10, 20, 30, 40}));
    auto it = numbers.begin();
    CHECK(it[2] == V(30) && (it + 3)->toInteger() == 40 && numbers.end() - numbers.begin() == 4);
    it += 2;
    CHECK(it->toInteger() == 30 && (it - 1)->toInteger() == 20 && it > numbers.begin() && it <= numbers.end());
    it = numbers.erase(it);
    CHECK(numbers.size() == 3 && it->toInteger() == 40);
    it = numbers.insert(it, V(35));
    CHECK(numbers == (QCborArray{10, 20, 35, 40}) && it->toInteger() == 35);
    CHECK(numbers.extract(numbers.begin()) == V(10) && numbers.size() == 3);
    QCborArray::ConstIterator cit = numbers.constBegin();
    CHECK((*cit).toInteger() == 20 && cit != numbers.constEnd() && cit == numbers.cbegin() &&
          numbers.cend() - cit == 3);
    QCborArray::ConstIterator fromMutable = numbers.begin();
    CHECK(fromMutable == numbers.constBegin() && numbers.begin() == numbers.constBegin());
    numbers.pop_front();
    numbers.pop_back();
    numbers.push_back(V(50));
    numbers.push_front(V(5));
    CHECK(numbers == (QCborArray{5, 35, 50}));
    numbers.removeFirst();
    numbers.removeLast();
    CHECK(numbers == QCborArray{35});
    const QCborArray plus = numbers + V(1);
    numbers += V(2);
    CHECK(plus == (QCborArray{35, 1}) && numbers == (QCborArray{35, 2}));
    numbers.clear();
    CHECK(numbers.isEmpty());

    // Copies are independent
    QCborArray original{1, QCborArray{2}};
    QCborArray copy = original;
    copy[0] = 100;
    CHECK(original.at(0) == V(1) && copy.at(0) == V(100));
    QCborArray swapped;
    swapped.swap(copy);
    CHECK(copy.isEmpty() && swapped.size() == 2 && swapped.toCborValue().isArray());

    // Conversions
    CHECK(QCborArray::fromStringList({u"a"_s, u"b"_s}) == (QCborArray{u"a"_s, u"b"_s}));
    const QVariantList variants{1, u"x"_s, true};
    CHECK(QCborArray::fromVariantList(variants) == (QCborArray{1, u"x"_s, true}));
    CHECK((QCborArray{1, u"x"_s}.toVariantList() == QVariantList{qlonglong(1), u"x"_s}));
    const QJsonArray json{1, 2.5, u"s"_s, true, QJsonValue()};
    CHECK(QCborArray::fromJsonArray(json) == (QCborArray{1, 2.5, u"s"_s, true, nullptr}));
    CHECK((QCborArray{1, 2.5, u"s"_s, nullptr}).toJsonArray() == (QJsonArray{1, 2.5, u"s"_s, QJsonValue()}));
}

static void testMap() {
    QCborMap map;
    CHECK(map.isEmpty() && map.size() == 0 && map.value(u"x"_s).isUndefined() && !map.contains(1));
    map.insert(u"name"_s, u"Nt"_s);
    map.insert(1, true);
    map.insert(QLatin1StringView("latin"), 2.5);
    map.insert(V(QByteArray("bytes")), V(nullptr));
    CHECK(map.size() == 4 && map.value(u"name"_s) == V(u"Nt"_s) && map.value(1).isTrue());
    CHECK(map.value(QLatin1StringView("name")) == V(u"Nt"_s) && map[QLatin1StringView("latin")] == V(2.5));
    CHECK(map.value(V(QByteArray("bytes"))).isNull() && map.contains(V(QByteArray("bytes"))));
    CHECK(map.contains(u"name"_s) && map.contains(QLatin1StringView("latin")) && !map.contains(2) &&
          !map.contains(V(1.0)));
    CHECK(map.keys() == (QList<QCborValue>{V(u"name"_s), V(1), V(u"latin"_s), V(QByteArray("bytes"))}));

    // operator[] adds, insert replaces
    map[u"new"_s] = 7;
    CHECK(map.size() == 5 && map.value(u"new"_s) == V(7));
    map[1] = false;
    map.insert(u"name"_s, u"Nt6"_s);
    CHECK(map.size() == 5 && map.value(1).isFalse() && map.value(u"name"_s) == V(u"Nt6"_s));
    QCborValueRef added = map[42];
    CHECK(added.isUndefined() && map.size() == 6);
    const QCborMap &constMap = map;
    CHECK(constMap[99].isUndefined() && map.size() == 6 && constMap[u"new"_s] == V(7));

    CHECK(map.take(u"new"_s) == V(7) && map.take(u"new"_s).isUndefined() && map.size() == 5);
    map.remove(42);
    map.remove(V(QByteArray("bytes")));
    map.remove(QLatin1StringView("latin"));
    map.remove(u"missing"_s);
    CHECK(map.size() == 2 && map.take(1).isFalse() && map.take(QLatin1StringView("name")) == V(u"Nt6"_s) &&
          map.isEmpty());

    // Iterators: pairs, keys and values
    QCborMap scores{{u"a"_s, 1}, {u"b"_s, 2}, {u"c"_s, 3}};
    QString keys;
    qint64 sum = 0;
    for (auto [key, value] : scores) {
        keys += key.toString();
        sum += value.toInteger();
    }
    CHECK(keys == u"abc"_s && sum == 6);
    for (auto it = scores.begin(); it != scores.end(); ++it)
        it.value() = it.value().toInteger() * 2;
    CHECK(scores.value(u"c"_s) == V(6));
    auto it = scores.find(u"b"_s);
    CHECK(it != scores.end() && it.key() == V(u"b"_s) && it->toInteger() == 4 && (*it).second.toInteger() == 4);
    *it.operator->() = 5; // through the reference that operator->() gives
    CHECK(scores.value(u"b"_s) == V(5));
    CHECK(scores.find(u"z"_s) == scores.end() && scores.constFind(u"z"_s) == scores.constEnd());
    CHECK(scores.constFind(QLatin1StringView("a")).value() == V(2) && scores.find(V(u"c"_s)).value() == V(6));
    const QCborMap &constScores = scores;
    CHECK(constScores.find(u"a"_s).key() == V(u"a"_s) && constScores.find(1) == constScores.end());
    CHECK(scores.end() - scores.begin() == 3 && (scores.begin() + 2).key() == V(u"c"_s) &&
          scores.begin()[1].first == V(u"b"_s));
    it = scores.erase(scores.find(u"a"_s));
    CHECK(scores.size() == 2 && it.key() == V(u"b"_s));
    CHECK(scores.extract(scores.find(u"b"_s)) == V(5) && scores.size() == 1);
    QCborMap::ConstIterator cit = scores.begin();
    CHECK(cit == scores.constBegin() && cit.key() == V(u"c"_s) && ++cit == scores.cend());
    const auto inserted = scores.insert({V(u"d"_s), V(8)});
    CHECK(inserted.key() == V(u"d"_s) && inserted.value() == V(8));

    // A key that comes twice in the list keeps its first value
    const QCborMap twice{{1, u"first"_s}, {1, u"second"_s}};
    CHECK(twice.size() == 1 && twice.value(1) == V(u"first"_s));

    // Nested changes through QCborValue
    V doc;
    doc[u"config"_s][u"depth"_s] = 3;
    doc[u"list"_s][2] = u"third"_s;
    CHECK(doc.isMap() && doc[u"config"_s][u"depth"_s] == V(3) && doc[u"list"_s].toArray().size() == 3);
    CHECK(doc[u"list"_s][0].isUndefined() && doc[u"list"_s][2] == V(u"third"_s));
    V copy = doc;
    copy[u"config"_s][u"depth"_s] = 4;
    CHECK(doc[u"config"_s][u"depth"_s] == V(3) && copy[u"config"_s][u"depth"_s] == V(4)); // shared until changed
    V arr(QCborArray{1, 2});
    arr[u"key"_s] = 5; // an array becomes a map of its indexes
    CHECK(arr.isMap() && arr[1] == V(2) && arr[u"key"_s] == V(5) && arr.toMap().size() == 3);
    V far(QCborArray{1});
    far[100000] = 2;
    CHECK(far.isMap() && far[0] == V(1) && far[100000] == V(2));
    V scalar(5);
    scalar[1] = 9;
    CHECK(scalar.isArray() && scalar.toArray() == (QCborArray{V(), 9}));

    // Conversions
    QVariantMap variants{{u"i"_s, 3}, {u"s"_s, u"x"_s}};
    const QCborMap fromVariants = QCborMap::fromVariantMap(variants);
    CHECK(fromVariants.value(u"i"_s) == V(3) && fromVariants.value(u"s"_s) == V(u"x"_s));
    CHECK(QCborMap::fromVariantHash(QVariantHash{{u"k"_s, true}}).value(u"k"_s).isTrue());
    const QCborMap mixed{{1, 2}, {u"s"_s, QByteArray("\xff")}, {QByteArray("\x01"), 3}, {V(true), 4}};
    const QVariantMap asVariants = mixed.toVariantMap();
    CHECK(asVariants.value(u"1"_s) == QVariant(qlonglong(2)) &&
          asVariants.value(u"s"_s) == QVariant(QByteArray("\xff")));
    CHECK(asVariants.value(u"AQ"_s) == QVariant(qlonglong(3)) && asVariants.value(u"true"_s) == QVariant(qlonglong(4)));
    CHECK(mixed.toVariantHash().size() == 4);
    const QJsonObject json = mixed.toJsonObject();
    CHECK(json.value(u"1"_s) == QJsonValue(2) && json.value(u"s"_s) == QJsonValue(u"_w"_s) && json.size() == 4);
    const QCborMap back = QCborMap::fromJsonObject(QJsonObject{{u"a"_s, 1}, {u"b"_s, QJsonArray{true}}});
    CHECK(back.value(u"a"_s) == V(1) && back.value(u"b"_s) == V(QCborArray{true}));
}

static void testEncoding() {
    CHECK(encode(V(0)) == hex("00") && encode(V(-1)) == hex("20") && encode(V(500)) == hex("1901f4"));
    CHECK(encode(V(std::numeric_limits<qint64>::min())) == hex("3b7fffffffffffffff"));
    CHECK(encode(V(1.5)) == hex("fb3ff8000000000000"));
    CHECK(encode(V(1.5), V::UseFloat) == hex("fa3fc00000") && encode(V(1.5), V::UseFloat16) == hex("f93e00"));
    CHECK(encode(V(1.1), V::UseFloat16) == hex("fb3ff199999999999a")); // no smaller form loses nothing
    CHECK(encode(V(100000.0), V::UseFloat16) == hex("fa47c35000"));
    CHECK(encode(V(qQNaN()), V::UseFloat16).size() == 3 && encode(V(qInf()), V::UseFloat) == hex("fa7f800000"));
    CHECK(encode(V(3.0), V::UseIntegers) == hex("03") && encode(V(-2.0), V::UseIntegers) == hex("21"));
    CHECK(encode(V(2.5), V::UseIntegers) == hex("fb4004000000000000"));
    CHECK(encode(V(u"\xfc"_s)) == hex("62c3bc") && encode(V(QByteArray("\x01\x02"))) == hex("420102"));
    CHECK(encode(V(QCborTag(55799), V(1))) == hex("d9d9f701"));
    CHECK(encode(V(V::Invalid)) == hex("f7"));
    // Sorted keys: by the encoded key, shorter first
    const QCborMap unsorted{{u"bb"_s, 1}, {100, 2}, {u"a"_s, 3}, {-1, 4}, {QCborArray{}, 5}};
    CHECK(encode(V(unsorted)) == hex("a5626262011864026161032004"
                                     "8005"));
    CHECK(encode(V(unsorted), V::SortKeysInMaps) == hex("a52004800518640261610362626201"));
    // Every value goes through the encoding and back
    const V everything(QCborMap{{u"int"_s, -123456789},
                                {u"double"_s, 3.25},
                                {u"text"_s, u"h\xe4llo"_s},
                                {u"bytes"_s, QByteArray("\x00\xff", 2)},
                                {u"list"_s, QCborArray{true, false, nullptr, V(), QCborArray{}, QCborMap{}}},
                                {u"date"_s, V(QDateTime(QDate(2020, 1, 2), QTime(3, 4, 5), Qt::UTC))},
                                {u"url"_s, V(QUrl(u"http://x.example/"_s))},
                                {u"uuid"_s, V(QUuid::createUuid())},
                                {u"tag"_s, V(QCborTag(99), V(1))},
                                {7, V(QCborSimpleType(99))}});
    for (V::EncodingOptions opt : {V::EncodingOptions(V::NoTransformation), V::EncodingOptions(V::SortKeysInMaps),
                                   V::EncodingOptions(V::UseFloat16)}) {
        QCborParserError error;
        const V again = V::fromCbor(everything.toCbor(opt), &error);
        // Sorting the keys changes the order of the pairs, which the comparison of maps counts
        bool same = error.error == QCborError::NoError && again.toMap().size() == everything.toMap().size();
        const QCborMap original = everything.toMap();
        for (auto it = original.constBegin(); same && it != original.constEnd(); ++it)
            same = again.toMap().value(it.key().concrete()) == it.value().concrete();
        CHECK(same && (opt.testFlag(V::SortKeysInMaps) || again == everything));
    }
    CHECK(V::fromCbor(everything.toCbor()).toMap().value(u"date"_s).isDateTime());
}

static void testParsingErrors() {
    const struct {
        const char *hex;
        QCborError::Code code;
    } broken[] = {
        {"", QCborError::EndOfFile},
        {"18", QCborError::EndOfFile},
        {"1900", QCborError::EndOfFile},
        {"62c3", QCborError::EndOfFile},
        {"8301", QCborError::EndOfFile},
        {"a1", QCborError::EndOfFile},
        {"a101", QCborError::EndOfFile},
        {"9f01", QCborError::EndOfFile},
        {"5f4101", QCborError::EndOfFile},
        {"0001", QCborError::GarbageAtEnd},
        {"ff", QCborError::UnexpectedBreak},
        {"83ff", QCborError::UnexpectedBreak},
        {"1c", QCborError::IllegalNumber},
        {"1f", QCborError::IllegalNumber},
        {"3f", QCborError::IllegalNumber},
        {"f800", QCborError::IllegalSimpleType},
        {"f81f", QCborError::IllegalSimpleType},
        {"fc", QCborError::UnknownType},
        {"5f6161ff", QCborError::IllegalType},
        {"5f5f4100ffff", QCborError::IllegalType},
        {"62c328", QCborError::InvalidUtf8String},
        {"63eda080", QCborError::InvalidUtf8String},
        {"62c080", QCborError::InvalidUtf8String},
        {"64f4908080", QCborError::InvalidUtf8String},
        {"5b7fffffffffffffff", QCborError::DataTooLarge},
    };
    for (const auto &b : broken) {
        QCborParserError error;
        V::fromCbor(hex(b.hex), &error);
        CHECK(error.error == b.code);
        if (error.error != b.code)
            std::printf("  %s: %d %s\n", b.hex, int(error.error.code()), error.errorString().toUtf8().constData());
    }
    QCborParserError error;
    const V partial = V::fromCbor(hex("830102"), &error);
    CHECK(error.error == QCborError::EndOfFile && error.offset == 3 && partial.isArray());
    V::fromCbor(hex("01ff"), &error);
    CHECK(error.error == QCborError::GarbageAtEnd && error.offset == 1);
    V::fromCbor(hex("01"), &error);
    CHECK(error.error == QCborError::NoError && error.offset == 0 && error.errorString() == u"No error"_s);

    // Nesting deeper than the limit
    QByteArray deep(2000, '\x81');
    deep.append('\x01');
    V::fromCbor(deep, &error);
    CHECK(error.error == QCborError::NestingTooDeep);
    QByteArray fine(500, '\x81');
    fine.append('\x01');
    V::fromCbor(fine, &error);
    CHECK(error.error == QCborError::NoError);

    CHECK(QCborError{QCborError::EndOfFile}.toString().contains(u"end of input"_s));
    for (QCborError::Code code :
         {QCborError::UnknownError, QCborError::AdvancePastEnd, QCborError::InputOutputError,
          QCborError::UnexpectedBreak, QCborError::UnknownType, QCborError::DataTooLarge, QCborError::UnsupportedType})
        CHECK(!QCborError{code}.toString().isEmpty());
    CHECK(QCborError{QCborError::IllegalType}.code() == QCborError::IllegalType);
}

static void testWriter() {
    QByteArray out;
    QCborStreamWriter writer(&out);
    CHECK(writer.device() == nullptr);
    writer.startMap(3);
    writer.append("text");
    writer.append(QLatin1StringView("latin \xe4"));
    writer.append(1);
    writer.append(quint64(18446744073709551615ULL));
    writer.append(u"neg"_s);
    writer.append(QCborNegativeInteger(0));
    CHECK(writer.endMap());
    CHECK(out == hex("a3647465787468"
                     "6c6174696e20c3a4"
                     "01"
                     "1bffffffffffffffff"
                     "636e6567"
                     "3bffffffffffffffff"));

    out.clear();
    writer.startArray();
    writer.append(true);
    writer.append(false);
    writer.append(nullptr);
    writer.appendUndefined();
    writer.append(QCborSimpleType(32));
    writer.append(qfloat16(1.5f));
    writer.append(1.5f);
    writer.append(QCborKnownTags::Uuid);
    writer.appendByteString("\x01\x02", 2);
    writer.appendTextString("ab", 2);
    writer.append(QByteArray("z"));
    writer.append(-1);
    writer.append(7u);
    writer.append(qint64(-500));
    writer.append(QStringView(u"v"));
    writer.append("xyz", 2);
    CHECK(writer.endArray());
    CHECK(out == hex("9f"
                     "f5f4f6f7"
                     "f820"
                     "f93e00"
                     "fa3fc00000"
                     "d825"
                     "420102"
                     "626162"
                     "417a"
                     "20"
                     "07"
                     "3901f3"
                     "6176"
                     "627879"
                     "ff"));

    // Counts that do not match
    out.clear();
    writer.startArray(2);
    writer.append(1);
    CHECK(!writer.endArray());
    CHECK(!writer.endMap()); // nothing open
    writer.startMap();
    writer.appendNull();
    writer.appendNull();
    CHECK(!writer.endArray()); // a map is open
    CHECK(writer.endMap());

    // To a device
    QBuffer buffer;
    CHECK(buffer.open(QIODevice::WriteOnly));
    QCborStreamWriter toDevice(&buffer);
    CHECK(toDevice.device() == &buffer);
    V(QCborArray{1, u"a"_s}).toCbor(toDevice);
    CHECK(buffer.data() == hex("82016161"));
    QBuffer second;
    CHECK(second.open(QIODevice::WriteOnly));
    toDevice.setDevice(&second);
    toDevice.append(5);
    CHECK(second.data() == hex("05") && buffer.data().size() == 4);
}

static void testReader() {
    // A walk through a document with every kind of item
    const QByteArray data = hex("a4"
                                "646e616d65"
                                "624e74"
                                "6474616773"
                                "9f01f93e00fa3fc00000fb3ff8000000000000ff"
                                "6162"
                                "5f4201024103ff"
                                "c1"
                                "1a514b67b0"
                                "f6");
    R reader(data);
    CHECK(reader.isValid() && reader.isMap() && reader.isContainer() && reader.isLengthKnown() && reader.length() == 4);
    CHECK(reader.containerDepth() == 0 && reader.parentContainerType() == R::Invalid && reader.currentOffset() == 0);
    CHECK(reader.enterContainer() && reader.containerDepth() == 1 && reader.parentContainerType() == R::Map);
    CHECK(reader.isString() && reader.length() == 4 && reader.readAllString() == u"name"_s);
    auto chunk = reader.readString(); // not a string any more: the reader is at the value
    CHECK(reader.isString());
    CHECK(chunk.status == R::Ok && chunk.data == u"Nt"_s);
    chunk = reader.readString();
    CHECK(chunk.status == R::EndOfString && reader.isString());
    CHECK(reader.readAllUtf8String() == QByteArray("tags"));
    CHECK(reader.isArray() && !reader.isLengthKnown() && reader.length() == 0 && reader.enterContainer());
    CHECK(reader.hasNext() && reader.isUnsignedInteger() && reader.isInteger() && reader.toUnsignedInteger() == 1 &&
          reader.toInteger() == 1);
    CHECK(reader.next() && reader.isFloat16() && float(reader.toFloat16()) == 1.5f && reader.type() == R::HalfFloat);
    CHECK(reader.next() && reader.isFloat() && reader.toFloat() == 1.5f);
    CHECK(reader.next() && reader.isDouble() && reader.toDouble() == 1.5);
    CHECK(reader.next() && !reader.hasNext() && !reader.next() && reader.lastError() == QCborError::AdvancePastEnd);
    // An error from going past the end stays, as in a stream that is broken
    CHECK(!reader.leaveContainer());

    // Again, without that mistake
    R again(data);
    again.enterContainer();
    again.next();
    again.next();
    again.next();
    CHECK(again.isArray() && again.next()); // skips the whole array
    CHECK(again.isString() && again.readAllString() == u"b"_s);
    CHECK(again.isByteArray() && again.readAllByteArray() == QByteArray("\x01\x02\x03"));
    CHECK(again.isTag() && again.toTag() == QCborTag(1) && again.next());
    CHECK(again.isUnsignedInteger() && again.toUnsignedInteger() == 1363896240 && again.next());
    CHECK(again.isNull() && again.next());
    CHECK(!again.hasNext() && again.leaveContainer() && again.containerDepth() == 0);
    CHECK(!again.isValid() && again.lastError() == QCborError::NoError && !again.hasNext());

    // Chunks of a string of unknown length
    R chunks(hex("7f6261626163ff"));
    auto first = chunks.readString();
    auto second = chunks.readString();
    auto end = chunks.readString();
    CHECK(first.status == R::Ok && first.data == u"ab"_s && second.status == R::Ok && second.data == u"c"_s);
    CHECK(end.status == R::EndOfString && !chunks.isValid() && chunks.lastError() == QCborError::NoError);
    R bytes(hex("5f410142020340ff"));
    CHECK(bytes.readByteArray().data == QByteArray("\x01") && bytes.readByteArray().data == QByteArray("\x02\x03"));
    CHECK(bytes.readByteArray().data.isEmpty() && bytes.readByteArray().status == R::EndOfString);
    CHECK(bytes.readByteArray().status == R::Error); // not at a byte string any more

    // Integers, simple types and the wrong accessors
    R numbers(hex("3bffffffffffffffff"
                  "20"
                  "f4f5f6f7f0"));
    CHECK(numbers.isNegativeInteger() && numbers.toNegativeInteger() == QCborNegativeInteger(0) && numbers.next());
    CHECK(numbers.toNegativeInteger() == QCborNegativeInteger(1) && numbers.toInteger() == -1 &&
          numbers.toUnsignedInteger() == 0);
    CHECK(numbers.next() && numbers.isFalse() && numbers.isBool() && !numbers.toBool() && numbers.next());
    CHECK(numbers.isTrue() && numbers.toBool() && numbers.next() && numbers.isNull() && numbers.next() &&
          numbers.isUndefined());
    CHECK(numbers.next() && numbers.isSimpleType(QCborSimpleType(16)) && numbers.toSimpleType() == QCborSimpleType(16));
    CHECK(numbers.toDouble() == 0 && numbers.toFloat() == 0 && numbers.toTag() == QCborTag(0) &&
          numbers.readAllString().isNull());
    CHECK(numbers.readAllByteArray().isNull() && numbers.readUtf8String().status == R::Error &&
          !numbers.enterContainer());

    // Pieces of data
    const QByteArray whole = hex("a2"
                                 "6161"
                                 "83010203"
                                 "6162"
                                 "781a"
                                 "6162636465666768696a6b6c6d6e6f707172737475767778797a");
    R incremental;
    CHECK(!incremental.isValid() && incremental.lastError() == QCborError::EndOfFile);
    QCborValue result;
    for (qsizetype i = 0; i < whole.size(); ++i) {
        incremental.addData(whole.mid(i, 1));
        incremental.reparse();
    }
    result = V::fromCbor(incremental);
    CHECK(incremental.lastError() == QCborError::NoError && result.toMap().value(u"b"_s).toString().size() == 26);
    // Interrupted in the middle of a string, and continued
    R partial(whole.left(12));
    CHECK(partial.enterContainer() && partial.readAllString() == u"a"_s && partial.next());
    CHECK(partial.readAllString() == u"b"_s);
    auto cut = partial.readString();
    CHECK(cut.status == R::Error && partial.lastError() == QCborError::EndOfFile);
    partial.addData(whole.mid(12));
    partial.reparse();
    cut = partial.readString();
    CHECK(cut.status == R::Ok && cut.data.size() == 26 && partial.readString().status == R::EndOfString);
    CHECK(!partial.hasNext() && partial.leaveContainer());
    // Several items at the top level, arriving after the first was read
    R stream(hex("01"));
    CHECK(stream.toInteger() == 1 && stream.next() && !stream.isValid() && stream.lastError() == QCborError::NoError);
    stream.addData(hex("02"));
    stream.reparse();
    CHECK(stream.isValid() && stream.toInteger() == 2);

    // Constructors, devices, reset() and clear()
    const char raw[] = "\x82\x01\x02";
    R fromChars(raw, 3);
    R fromBytes(reinterpret_cast<const quint8 *>(raw), 3);
    CHECK(V::fromCbor(fromChars) == V(QCborArray{1, 2}) && V::fromCbor(fromBytes) == V(QCborArray{1, 2}));
    fromChars.reset();
    CHECK(fromChars.isArray() && fromChars.currentOffset() == 0);
    fromChars.clear();
    CHECK(!fromChars.isValid() && fromChars.device() == nullptr);
    fromChars.addData(raw, 1);
    fromChars.addData(reinterpret_cast<const quint8 *>(raw + 1), 2);
    fromChars.reparse();
    CHECK(V::fromCbor(fromChars) == V(QCborArray{1, 2}));
    QByteArray deviceData = hex("8301") + hex("0203");
    QBuffer buffer(&deviceData);
    CHECK(buffer.open(QIODevice::ReadOnly));
    R fromDevice(&buffer);
    CHECK(fromDevice.device() == &buffer && V::fromCbor(fromDevice) == V(QCborArray{1, 2, 3}));
    QBuffer other(&deviceData);
    CHECK(other.open(QIODevice::ReadOnly));
    fromDevice.setDevice(&other);
    CHECK(fromDevice.device() == &other && fromDevice.isArray());

    // next() with a limit on the nesting
    R nested(hex("818181818101"));
    CHECK(!nested.next(3) && nested.lastError() == QCborError::NestingTooDeep);
    R nestedFine(hex("818181818101"));
    CHECK(nestedFine.next(10) && !nestedFine.isValid() && nestedFine.lastError() == QCborError::NoError);
}

static void testConversions() {
    // To and from QVariant
    CHECK(V::fromVariant(QVariant()).isUndefined() && V::fromVariant(QVariant(nullptr)).isNull());
    CHECK(V::fromVariant(QVariant(true)).isTrue() && V::fromVariant(QVariant(42)) == V(42) &&
          V::fromVariant(QVariant(2.5)) == V(2.5));
    CHECK(V::fromVariant(QVariant::fromValue(qulonglong(18446744073709551615ULL))).isDouble());
    CHECK(V::fromVariant(QVariant::fromValue(short(-3))) == V(-3) &&
          V::fromVariant(QVariant::fromValue(1.5f)) == V(1.5));
    CHECK(V::fromVariant(QVariant(u"s"_s)) == V(u"s"_s) &&
          V::fromVariant(QVariant(QByteArray("b"))) == V(QByteArray("b")));
    CHECK(V::fromVariant(QVariant(QStringList{u"a"_s})) == V(QCborArray{u"a"_s}));
    const QDateTime when(QDate(2021, 5, 6), QTime(7, 8, 9), Qt::UTC);
    CHECK(V::fromVariant(QVariant(when)).toDateTime() == when &&
          V::fromVariant(QVariant(QUrl(u"http://a/"_s))).isUrl());
    CHECK(V::fromVariant(QVariant(QUuid::createUuid())).isUuid() &&
          V::fromVariant(QVariant(QRegularExpression(u"x"_s))).isRegularExpression());
    CHECK(V::fromVariant(QVariant(QVariantList{1, u"x"_s})) == V(QCborArray{1, u"x"_s}));
    CHECK(V::fromVariant(QVariant(QVariantMap{{u"k"_s, 1}})) == V(QCborMap{{u"k"_s, 1}}));
    CHECK(V::fromVariant(QVariant(QVariantHash{{u"k"_s, 1}})) == V(QCborMap{{u"k"_s, 1}}));
    CHECK(V::fromVariant(QVariant(QJsonValue(5))) == V(5));
    CHECK(V::fromVariant(QVariant::fromValue(QChar(u'c'))) == V(u"c"_s));
    CHECK(V::fromVariant(QVariant(QPoint(1, 2))).isUndefined());

    CHECK(V(5).toVariant() == QVariant(qlonglong(5)) && V(5).toVariant().typeId() == QMetaType::LongLong);
    CHECK(V(2.5).toVariant() == QVariant(2.5) && V(true).toVariant() == QVariant(true));
    CHECK(V(nullptr).toVariant().typeId() == QMetaType::Nullptr && !V().toVariant().isValid());
    CHECK(V(u"s"_s).toVariant() == QVariant(u"s"_s) && V(QByteArray("b")).toVariant() == QVariant(QByteArray("b")));
    CHECK(V(when).toVariant() == QVariant(when) && V(QUrl(u"http://a/"_s)).toVariant().typeId() == QMetaType::QUrl);
    CHECK(V(QCborArray{1}).toVariant() == QVariant(QVariantList{qlonglong(1)}));
    CHECK(V(QCborMap{{u"k"_s, 1}}).toVariant() == QVariant(QVariantMap{{u"k"_s, qlonglong(1)}}));
    const QVariant tagVariant = V(QCborTag(77), V(1)).toVariant();
    CHECK(tagVariant.typeId() == QMetaType::QCborValue && tagVariant.value<QCborValue>() == V(QCborTag(77), V(1)));
    CHECK(V::fromVariant(tagVariant) == V(QCborTag(77), V(1)));
    const QVariant simpleVariant = V(QCborSimpleType(40)).toVariant();
    CHECK(simpleVariant.typeId() == QMetaType::QCborSimpleType &&
          V::fromVariant(simpleVariant) == V(QCborSimpleType(40)));
    CHECK(QVariant::fromValue(QCborArray{1}).typeId() == QMetaType::QCborArray);
    CHECK(V::fromVariant(QVariant::fromValue(QCborArray{1})) == V(QCborArray{1}));
    CHECK(V::fromVariant(QVariant::fromValue(QCborMap{{1, 2}})) == V(QCborMap{{1, 2}}));
    CHECK(QMetaType(QMetaType::QCborValue).name() == QByteArray("QCborValue"));

    // To and from JSON
    CHECK(V::fromJsonValue(QJsonValue()) == V(nullptr) &&
          V::fromJsonValue(QJsonValue(QJsonValue::Undefined)).isUndefined());
    CHECK(V::fromJsonValue(QJsonValue(3.0)) == V(3) && V::fromJsonValue(QJsonValue(3.5)) == V(3.5) &&
          V::fromJsonValue(QJsonValue(1e300)).isDouble());
    CHECK(V::fromJsonValue(QJsonValue(true)).isTrue() && V::fromJsonValue(QJsonValue(u"s"_s)) == V(u"s"_s));
    CHECK(V::fromJsonValue(QJsonValue(QJsonArray{1})) == V(QCborArray{1}));
    CHECK(V::fromJsonValue(QJsonValue(QJsonObject{{u"k"_s, 1}})) == V(QCborMap{{u"k"_s, 1}}));
    CHECK(V(5).toJsonValue() == QJsonValue(5) && V(2.5).toJsonValue() == QJsonValue(2.5) &&
          V(false).toJsonValue() == QJsonValue(false));
    CHECK(V(qInf()).toJsonValue().isNull() && V(qQNaN()).toJsonValue().isNull() && V().toJsonValue().isNull());
    CHECK(V(nullptr).toJsonValue().isNull() && V(V::Invalid).toJsonValue().isNull());
    CHECK(V(QByteArray("\xfb\xff")).toJsonValue() == QJsonValue(u"-_8"_s));
    CHECK(V(QCborKnownTags::ExpectedBase64, V(QByteArray("\xfb\xff"))).toJsonValue() == QJsonValue(u"+/8="_s));
    CHECK(V(QCborKnownTags::ExpectedBase16, V(QByteArray("\xfb\xff"))).toJsonValue() == QJsonValue(u"fbff"_s));
    CHECK(V(QCborKnownTags::ExpectedBase64url, V(QByteArray("\xfb\xff"))).toJsonValue() == QJsonValue(u"-_8"_s));
    CHECK(V(QCborTag(1), V(5)).toJsonValue() == QJsonValue(5));
    CHECK(V(QUrl(u"http://a/b"_s)).toJsonValue() == QJsonValue(u"http://a/b"_s));
    const QUuid uuid = QUuid::fromString(u"{67c8770b-44f1-410a-ab9a-f9b5446f13ee}"_s);
    CHECK(V(uuid).toJsonValue() == QJsonValue(u"67c8770b-44f1-410a-ab9a-f9b5446f13ee"_s));
    CHECK(V(QRegularExpression(u"a+"_s)).toJsonValue() == QJsonValue(u"a+"_s));
    CHECK(V(QCborSimpleType(40)).toJsonValue() == QJsonValue(u"simple(40)"_s));
    CHECK(V(QCborArray{1, u"x"_s}).toJsonValue() == QJsonValue(QJsonArray{1, u"x"_s}));
}

static void testDiagnosticNotation() {
    CHECK(V(QCborArray{1, u"two"_s, QByteArray("\x03")}).toDiagnosticNotation() == u"[1, \"two\", h'03']"_s);
    CHECK(V(2.0).toDiagnosticNotation() == u"2.0"_s && V(0.1).toDiagnosticNotation() == u"0.1"_s &&
          V(-1e-10).toDiagnosticNotation() == u"-1e-10"_s);
    CHECK(V(1.5).toDiagnosticNotation(V::ExtendedFormat) == u"1.5_3"_s);
    CHECK(V(u"q\"\\\n\t\x01"_s).toDiagnosticNotation() == u"\"q\\\"\\\\\\n\\t\\u0001\""_s);
    CHECK(V(QCborTag(42), V(QCborMap{{1, V(QCborSimpleType(99))}})).toDiagnosticNotation() == u"42({1: simple(99)})"_s);
    CHECK(V(V::Invalid).toDiagnosticNotation() == u"<invalid>"_s && V().toDiagnosticNotation() == u"undefined"_s);
    const V nested(QCborMap{{u"a"_s, QCborArray{1, 2}}, {u"b"_s, QCborMap{}}});
    CHECK(nested.toDiagnosticNotation() == u"{\"a\": [1, 2], \"b\": {}}"_s);
    CHECK(nested.toDiagnosticNotation(V::LineWrapped) ==
          u"{\n    \"a\": [\n        1,\n        2\n    ],\n    \"b\": {}\n}"_s);
    CHECK(diag(hex("c074323031332d30332d32315432303a30343a30305a")) == u"0(\"2013-03-21T20:04:00Z\")"_s);
}

static void testRefs() {
    QCborArray array{QCborMap{{u"k"_s, 1}}, QCborArray{1, 2}, u"s"_s};
    QCborValueRef map = array[0];
    CHECK(map.isMap() && map.isContainer() && map[u"k"_s] == V(1) && map[QLatin1StringView("k")] == V(1) &&
          map.toMap().size() == 1);
    map[u"k"_s] = 2;
    map[QLatin1StringView("l")] = 3;
    CHECK(array.at(0).toMap().value(u"k"_s) == V(2) && array.at(0).toMap().value(u"l"_s) == V(3));
    QCborValueRef inner = array[1];
    inner[0] = 10;
    CHECK(array.at(1) == V(QCborArray{10, 2}) && inner[1] == V(2) && inner.toArray().size() == 2);
    const QCborValueConstRef constRef = *array.constBegin();
    CHECK(constRef.isMap() && constRef.concrete().isMap() && constRef[u"k"_s] == V(2) &&
          constRef[qint64(0)].isUndefined());
    CHECK(constRef.toCbor().size() > 0 && !constRef.toDiagnosticNotation().isEmpty() && constRef.toVariant().isValid());
    CHECK(constRef.toJsonValue().isObject() && constRef.compare(V(1)) > 0 && constRef != V(1) &&
          constRef.toMap(QCborMap()).size() == 2);
    QByteArray out;
    QCborStreamWriter writer(&out);
    constRef.toCbor(writer);
    CHECK(out == constRef.toCbor());
    const QCborValueRef s = array[2];
    CHECK(s.isString() && s.toString() == u"s"_s && !s.isByteArray() && !s.isInteger() && !s.isArray() && !s.isTag());
    CHECK(!s.isFalse() && !s.isTrue() && !s.isBool() && !s.isNull() && !s.isUndefined() && !s.isDouble() &&
          !s.isDateTime());
    CHECK(!s.isUrl() && !s.isRegularExpression() && !s.isUuid() && !s.isInvalid() && !s.isSimpleType() &&
          !s.isSimpleType(QCborSimpleType::Null));
    CHECK(s.toInteger(3) == 3 && s.toBool(true) && s.toDouble(2) == 2 &&
          s.toSimpleType() == QCborSimpleType::Undefined);
    CHECK(s.tag() == QCborTag(~0ULL) && s.taggedValue().isUndefined() && s.toByteArray("x") == QByteArray("x"));
    CHECK(!s.toDateTime().isValid() && s.toUrl().isEmpty() && s.toUuid().isNull() &&
          s.toRegularExpression().pattern().isEmpty());
    CHECK(s.toArray().isEmpty() && s.toArray(QCborArray{1}).size() == 1 && s.toMap().isEmpty());
    CHECK(s < V(u"t"_s) && s == V(u"s"_s));
}

int main() {
    testRfcExamples();
    testValues();
    testCompare();
    testArray();
    testMap();
    testEncoding();
    testParsingErrors();
    testWriter();
    testReader();
    testConversions();
    testDiagnosticNotation();
    testRefs();

    if (g_failures) {
        std::printf("%d CBOR test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all CBOR tests passed");
    return 0;
}
