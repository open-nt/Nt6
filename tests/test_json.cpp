// MIT License - see LICENSE
#include <QByteArray>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QJsonValueConstRef>
#include <QJsonValueRef>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QDate>
#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QByteArray B(const char *s) { return QByteArray(s); }

static QJsonDocument parse(const QByteArray &json, QJsonParseError::ParseError *err = nullptr, int *offset = nullptr) {
    QJsonParseError e;
    e.error = QJsonParseError::GarbageAtEnd;
    e.offset = -5;
    QJsonDocument doc = QJsonDocument::fromJson(json, &e);
    if (err)
        *err = e.error;
    if (offset)
        *offset = e.offset;
    return doc;
}

static bool fails(const QByteArray &json, QJsonParseError::ParseError expected, int expectedOffset) {
    QJsonParseError::ParseError err = QJsonParseError::NoError;
    int offset = -1;
    const QJsonDocument doc = parse(json, &err, &offset);
    const bool ok = doc.isNull() && err == expected && offset == expectedOffset;
    if (!ok)
        std::printf("  input '%s': error %d offset %d (expected %d at %d)\n", json.constData(), int(err), offset,
                    int(expected), expectedOffset);
    return ok;
}

static void testValueBasics() {
    QJsonValue n;
    CHECK(n.isNull() && n.type() == QJsonValue::Null && !n.isUndefined());
    CHECK(QJsonValue(QJsonValue::Undefined).isUndefined());
    CHECK(QJsonValue(QJsonValue::Bool).type() == QJsonValue::Bool && !QJsonValue(QJsonValue::Bool).toBool(true));
    CHECK(QJsonValue(QJsonValue::Double).toDouble(5) == 0.0);
    CHECK(QJsonValue(QJsonValue::String).isString() && QJsonValue(QJsonValue::String).toString().isEmpty());
    CHECK(!QJsonValue(QJsonValue::String).toString().isNull());
    CHECK(QJsonValue(QJsonValue::Array).toArray().isEmpty());
    CHECK(QJsonValue(QJsonValue::Object).toObject().isEmpty());

    QJsonValue b(true);
    CHECK(b.isBool() && b.toBool() && !b.isNull() && !b.isDouble());
    CHECK(b.toInt(7) == 7 && b.toString("x") == "x" && b.toDouble(1.5) == 1.5);
    CHECK(QJsonValue(false).toBool(true) == false);
    CHECK(QJsonValue(1).toBool(true) == true);

    QJsonValue i(42);
    CHECK(i.isDouble() && i.toInt() == 42 && i.toInteger() == 42 && i.toDouble() == 42.0);
    CHECK(i.toString("d") == "d" && i.toString().isNull());
    QJsonValue d(2.5);
    CHECK(d.isDouble() && d.toDouble() == 2.5 && d.toInt(9) == 9 && d.toInteger(9) == 9);
    QJsonValue whole(3.0);
    CHECK(whole.toInt(9) == 3 && whole.toInteger(9) == 3);
    CHECK(QJsonValue(4294967296.0).toInt(9) == 9 && QJsonValue(4294967296.0).toInteger() == 4294967296LL);
    CHECK(QJsonValue(qint64(4294967296LL)).toInt(9) == 9);
    CHECK(QJsonValue(-2147483648.0).toInt(9) == -2147483648);
    CHECK(QJsonValue(2147483648.0).toInt(9) == 9);
    CHECK(QJsonValue(1e30).toInteger(9) == 9);
    CHECK(QJsonValue("x").toDouble(2) == 2);

    // Exact integers up to 2^53, doubles beyond.
    const qint64 p53 = qint64(1) << 53;
    CHECK(QJsonValue(p53).toInteger() == p53 && QJsonValue(-p53).toInteger() == -p53);
    CHECK(QJsonValue(p53 + 1).toDouble() == static_cast<double>(p53 + 1));
    CHECK(QJsonValue(p53 + 1).toInteger() == p53);
    CHECK(QJsonValue(std::numeric_limits<qint64>::max()).toDouble() == 9223372036854775808.0);
    CHECK(QJsonValue(std::numeric_limits<qint64>::max()).toInteger(5) == 5);
    CHECK(QJsonValue(std::numeric_limits<qint64>::min()).toInteger(5) == std::numeric_limits<qint64>::min());

    // Non-finite numbers become null.
    CHECK(QJsonValue(std::numeric_limits<double>::quiet_NaN()).isNull());
    CHECK(QJsonValue(std::numeric_limits<double>::infinity()).isNull());
    CHECK(QJsonValue(-std::numeric_limits<double>::infinity()).isNull());

    QJsonValue s("héllo");
    CHECK(s.isString() && s.toString() == QString::fromUtf8("héllo"));
    CHECK(QJsonValue(QString("q")).toString() == "q");
    CHECK(QJsonValue(QLatin1StringView("lat")).toString() == "lat");
    CHECK(QJsonValue(QString()).isString() && QJsonValue(QString()).toString().isEmpty());
    CHECK(s.toInt(3) == 3 && s.toBool(true) && !s.toArray().size() && s.toObject().isEmpty());

    QJsonArray arr{1, 2};
    QJsonValue av(arr);
    CHECK(av.isArray() && av.toArray() == arr && av.toObject().isEmpty());
    CHECK(av.toArray(QJsonArray{9}).size() == 2 && QJsonValue(1).toArray(QJsonArray{9}).size() == 1);
    QJsonObject obj{{"k", 1}};
    QJsonValue ov(obj);
    CHECK(ov.isObject() && ov.toObject() == obj && ov.toArray().isEmpty());
    CHECK(QJsonValue(1).toObject(obj) == obj);

    CHECK(av[0] == 1 && av[1].toInt() == 2 && av[2].isUndefined() && av[-1].isUndefined());
    CHECK(ov["k"] == 1 && ov["missing"].isUndefined() && ov[QString("k")] == 1);
    CHECK(ov[QLatin1StringView("k")] == 1 && ov[QStringView(QString("k"))] == 1);
    CHECK(QJsonValue(5)["k"].isUndefined() && QJsonValue(5)[0].isUndefined());
    CHECK(av["k"].isUndefined() && ov[0].isUndefined());

    // Copy, move and swap.
    QJsonValue c = av;
    CHECK(c == av);
    QJsonValue m = std::move(c);
    CHECK(m == av);
    QJsonValue x(1), y("y");
    x.swap(y);
    CHECK(x.toString() == "y" && y.toInt() == 1);
    x = 7;
    CHECK(x.toInt() == 7);
}

static void testValueComparison() {
    CHECK(QJsonValue() == QJsonValue());
    CHECK(QJsonValue(QJsonValue::Undefined) == QJsonValue(QJsonValue::Undefined));
    CHECK(QJsonValue() != QJsonValue(QJsonValue::Undefined));
    CHECK(QJsonValue(1) == QJsonValue(1.0));
    CHECK(QJsonValue(1) != QJsonValue(2));
    CHECK(QJsonValue(0.1) == QJsonValue(0.1) && QJsonValue(0.1) != QJsonValue(0.2));
    CHECK(QJsonValue(true) == QJsonValue(true) && QJsonValue(true) != QJsonValue(false));
    CHECK(QJsonValue(1) != QJsonValue(true) && QJsonValue(0) != QJsonValue(false));
    CHECK(QJsonValue("a") == QJsonValue("a") && QJsonValue("a") != QJsonValue("b"));
    CHECK(QJsonValue("1") != QJsonValue(1));
    CHECK(QJsonValue(QJsonArray{1, "a"}) == QJsonValue(QJsonArray{1, "a"}));
    CHECK(QJsonValue(QJsonArray{1, "a"}) != QJsonValue(QJsonArray{1, "b"}));
    CHECK(QJsonValue(QJsonArray()) != QJsonValue(QJsonObject()));
    CHECK(QJsonValue(QJsonObject{{"a", 1}}) == QJsonValue(QJsonObject{{"a", 1.0}}));
    CHECK(QJsonValue(QJsonObject{{"a", 1}}) != QJsonValue(QJsonObject{{"b", 1}}));
    const qint64 p53 = qint64(1) << 53;
    CHECK(QJsonValue(p53) == QJsonValue(static_cast<double>(p53)));
    CHECK(QJsonValue(p53 + 2) == QJsonValue(static_cast<double>(p53 + 2)));
    CHECK(QJsonValue(-0.0) == QJsonValue(0));
}

static void testConstRefAndRef() {
    QJsonArray a{1, "two", QJsonArray{3}, QJsonObject{{"k", 4}}, true, QJsonValue()};
    const QJsonArray &ca = a;
    auto it = ca.begin();
    QJsonValueConstRef r0 = *it;
    CHECK(r0.isDouble() && r0.toInt() == 1 && r0.toDouble() == 1.0 && r0.toInteger() == 1);
    CHECK((*(it + 1)).isString() && (*(it + 1)).toString() == "two" && (*(it + 1)).toString("d") == "two");
    CHECK((*(it + 2)).isArray() && (*(it + 2)).toArray().size() == 1 && (*(it + 2))[0] == 3);
    CHECK((*(it + 3)).isObject() && (*(it + 3)).toObject().size() == 1 && (*(it + 3))["k"] == 4);
    CHECK((*(it + 3))[QLatin1StringView("k")] == 4 && (*(it + 3))[QStringView(QString("k"))] == 4);
    CHECK((*(it + 4)).isBool() && (*(it + 4)).toBool());
    CHECK((*(it + 5)).isNull() && !(*(it + 5)).isUndefined());
    CHECK((*(it + 5)).toArray(QJsonArray{1}).size() == 1 && (*(it + 5)).toObject(QJsonObject{{"a", 1}}).size() == 1);
    QJsonValue copy = *it;
    CHECK(copy == 1);
    CHECK(*it == QJsonValue(1) && QJsonValue(1) == *it && *it != QJsonValue(2) && QJsonValue(2) != *it);
    CHECK(*it == *it && *it != *(it + 1));
    CHECK(it->toInt() == 1);
    CHECK(r0 == 1);

    QJsonValueRef ref = a[1];
    CHECK(ref.toString() == "two");
    ref = QJsonValue(5);
    CHECK(a[1] == 5);
    a[0] = a[1];
    CHECK(a[0] == 5);
    QJsonValueRef r2(a[2]);
    r2 = "x";
    CHECK(a.at(2) == "x");
    a[3] = QJsonArray{1, 2};
    CHECK(a.at(3).toArray().size() == 2);

    // A reference past the end reads undefined, and assigning to it changes nothing.
    QJsonArray small{1};
    QJsonValueRef bad = small[5];
    CHECK(bad.isUndefined());
    bad = 7;
    CHECK(small.size() == 1 && small.at(0) == 1);

    // A reference reads the element it refers to when it is used.
    QJsonArray big{1, 2, 3};
    QJsonValueRef mid = big[1];
    big[1] = 20;
    CHECK(mid.toInt() == 20);
}

static void testArray() {
    QJsonArray a;
    CHECK(a.isEmpty() && a.empty() && a.size() == 0 && a.count() == 0 && a.length() == 0);
    CHECK(a.at(0).isUndefined() && a.first().isUndefined() && a.last().isUndefined());
    CHECK(a.begin() == a.end() && a.cbegin() == a.cend() && a.constBegin() == a.constEnd());
    a.append(1);
    a.append("two");
    a.prepend(0);
    a.insert(3, true);
    a.insert(4, QJsonValue());
    CHECK(a.size() == 5);
    CHECK(a.at(0) == 0 && a.at(1) == 1 && a.at(2) == "two" && a.at(3) == true && a.at(4).isNull());
    CHECK(a.first() == 0 && a.last().isNull());
    CHECK(a[1] == 1 && a.at(5).isUndefined() && a.at(-1).isUndefined());
    const QJsonArray &ca = a;
    CHECK(ca[2] == "two");
    a.insert(-1, 5);
    a.insert(6, 5);
    CHECK(a.size() == 5);
    a.insert(5, 9);
    CHECK(a.size() == 6 && a.last() == 9);
    a.replace(0, "zero");
    a.replace(99, "no");
    a.replace(-1, "no");
    CHECK(a.at(0) == "zero" && a.size() == 6);
    CHECK(a.contains("two") && a.contains(QJsonValue()) && !a.contains(12));
    a.removeAt(0);
    a.removeAt(10);
    a.removeAt(-1);
    CHECK(a.size() == 5 && a.first() == 1);
    QJsonValue t = a.takeAt(1);
    CHECK(t == "two" && a.size() == 4);
    CHECK(a.takeAt(10).isUndefined() && a.takeAt(-1).isUndefined());
    a.removeFirst();
    CHECK(a.first() == true);
    a.removeLast();
    CHECK(a.last().isNull() || a.last() == true);
    a.removeLast();
    a.removeLast();
    a.removeLast();
    CHECK(a.isEmpty());

    // Operators and STL aliases.
    QJsonArray b;
    b << 1 << 2;
    b += 3;
    b.push_back(4);
    b.push_front(0);
    CHECK(b.size() == 5 && b.first() == 0 && b.last() == 4);
    b.pop_front();
    b.pop_back();
    CHECK(b == (QJsonArray{1, 2, 3}));
    QJsonArray c = b + 9;
    CHECK(c.size() == 4 && b.size() == 3 && c.last() == 9);
    b.first() = 10;
    b.last() = 30;
    CHECK(b == (QJsonArray{10, 2, 30}));
    CHECK(b != c && b == b && QJsonArray() == QJsonArray());

    QJsonArray fromList = QJsonArray::fromStringList(QStringList{"a", "b"});
    CHECK(fromList.size() == 2 && fromList.at(1) == "b" && fromList.at(0).isString());
    CHECK(QJsonArray::fromStringList(QStringList()).isEmpty());

    // Initializer lists and nesting.
    QJsonArray n{1, "s", QJsonArray{1, 2}, QJsonObject{{"a", QJsonArray{}}}, QJsonValue(), 2.5, false};
    CHECK(n.size() == 7 && n.at(2).toArray().size() == 2 && n.at(3).toObject().value("a").isArray());
    CHECK(QJsonArray{}.isEmpty());
    // An empty array is equal to an array that had its elements removed.
    QJsonArray gone{1};
    gone.removeAt(0);
    CHECK(gone == QJsonArray() && QJsonArray() == gone);
    CHECK(QJsonValue(gone) == QJsonValue(QJsonArray()));
}

static void testArraySharing() {
    QJsonArray a{1, 2, 3};
    QJsonArray b = a;
    b.append(4);
    CHECK(a.size() == 3 && b.size() == 4);
    QJsonArray c = b;
    c[0] = 100;
    CHECK(b.at(0) == 1 && c.at(0) == 100);
    QJsonArray d = c;
    d.removeAt(0);
    CHECK(c.size() == 4 && d.size() == 3);

    // A value shares the array's data too, and detaches on write.
    QJsonValue v(a);
    a.append(9);
    CHECK(v.toArray().size() == 3 && a.size() == 4);
    QJsonArray fromV = v.toArray();
    fromV[0] = 77;
    CHECK(v.toArray().at(0) == 1);

    // Appending an array to itself copies it.
    QJsonArray self{1};
    self.append(self);
    CHECK(self.size() == 2 && self.at(1).toArray().size() == 1);
    self[0] = self;
    CHECK(self.at(0).toArray().size() == 2);

    // Copies made while an iterator is alive keep their own data.
    QJsonArray e{1, 2};
    auto it = e.begin();
    QJsonArray f = e;
    *it = 5;
    CHECK(e.at(0) == 5 && f.at(0) == 1);
    QJsonArray g = std::move(e);
    CHECK(g.size() == 2 && e.isEmpty());
    e = g;
    CHECK(e == g);
    QJsonArray h;
    h = std::move(g);
    CHECK(h.size() == 2);
    h.swap(e);
    e.swap(h);
}

static void testArrayIterators() {
    QJsonArray a{1, 2, 3, 4, 5};
    int sum = 0;
    for (const QJsonValue &v : a)
        sum += v.toInt();
    CHECK(sum == 15);
    sum = 0;
    for (auto v : a)
        sum += v.toInt();
    CHECK(sum == 15);
    for (QJsonValueRef v : a)
        v = v.toInt() * 2;
    CHECK(a == (QJsonArray{2, 4, 6, 8, 10}));
    const QJsonArray &ca = a;
    sum = 0;
    for (QJsonValue v : ca)
        sum += v.toInt();
    CHECK(sum == 30);

    auto it = a.begin();
    CHECK((*it).toInt() == 2 && it->toInt() == 2 && it[2].toInt() == 6);
    ++it;
    CHECK(it->toInt() == 4);
    CHECK((it++)->toInt() == 4 && it->toInt() == 6);
    CHECK((it--)->toInt() == 6 && it->toInt() == 4);
    --it;
    CHECK(it == a.begin());
    it += 3;
    CHECK(it->toInt() == 8);
    it -= 2;
    CHECK(it->toInt() == 4);
    CHECK((it + 2)->toInt() == 8 && (it - 1)->toInt() == 2 && (2 + it)->toInt() == 8);
    CHECK(a.end() - a.begin() == 5 && a.begin() < a.end() && a.end() > a.begin());
    CHECK(a.begin() <= a.begin() && a.end() >= a.end() && a.begin() != a.end());
    QJsonArray::const_iterator cit = a.begin();
    CHECK(cit == a.constBegin() && a.begin() == a.constBegin() && !(a.begin() != a.constBegin()));
    CHECK(a.constBegin() == a.begin() && a.begin() < a.constEnd() && a.constEnd() > a.begin());
    CHECK(a.begin() <= a.constBegin() && a.begin() >= a.constBegin());
    CHECK(cit[1].toInt() == 4 && (cit + 1)->toInt() == 4 && (cit + 4 - cit) == 4);
    CHECK((cit++)->toInt() == 2 && (++cit)->toInt() == 6 && (cit--)->toInt() == 6 && (--cit)->toInt() == 2);
    cit += 2;
    cit -= 1;
    CHECK(cit->toInt() == 4 && (3 + cit)->toInt() == 10 && (cit + 3)->toInt() == 10 && (cit + 2 - 1)->toInt() == 6);
    cit -= 1;
    CHECK(cit < cit + 1 && cit <= cit && cit + 1 > cit && cit >= cit);

    // Reverse iteration.
    std::string order;
    for (auto r = a.rbegin(); r != a.rend(); ++r)
        order += std::to_string(r->toInt()) + ",";
    CHECK(order == "10,8,6,4,2,");
    order.clear();
    for (auto r = a.crbegin(); r != a.crend(); ++r)
        order += std::to_string((*r).toInt()) + ",";
    CHECK(order == "10,8,6,4,2,");

    // STL algorithms.
    QJsonArray s{3, 1, 2};
    CHECK(std::distance(s.begin(), s.end()) == 3);
    auto found = std::find(s.constBegin(), s.constEnd(), QJsonValue(1));
    CHECK(found - s.constBegin() == 1);
    CHECK(std::count_if(s.begin(), s.end(), [](const QJsonValue &v) { return v.toInt() > 1; }) == 2);
    CHECK(std::accumulate(s.constBegin(), s.constEnd(), 0,
                          [](int acc, const QJsonValue &v) { return acc + v.toInt(); }) == 6);
    QJsonArray copy;
    std::copy(s.begin(), s.end(), std::back_inserter(copy));
    CHECK(copy == s);

    // Insert and erase through iterators.
    QJsonArray m{1, 2, 3};
    auto at = m.insert(m.begin() + 1, 10);
    CHECK(at - m.begin() == 1 && at->toInt() == 10 && m.size() == 4);
    at = m.erase(at);
    CHECK(at->toInt() == 2 && m.size() == 3);
    at = m.erase(m.end() - 1);
    CHECK(at == m.end() && m.size() == 2);
    at = m.insert(m.end(), 7);
    CHECK(m == (QJsonArray{1, 2, 7}) && at->toInt() == 7);
    m.insert(m.begin(), 0);
    CHECK(m.first() == 0);

    // Assigning through an iterator keeps every iterator valid.
    QJsonArray k{1, 2, 3};
    auto k0 = k.begin();
    auto k2 = k.begin() + 2;
    *k0 = 10;
    *k2 = 30;
    CHECK(k0->toInt() == 10 && k2->toInt() == 30 && k0 + 2 == k2);
    k.append(4);
    CHECK(k.begin() == k0 && k.begin()->toInt() == 10);
    CHECK((k.begin() + 3)->toInt() == 4);
}

static void testObject() {
    QJsonObject o;
    CHECK(o.isEmpty() && o.empty() && o.size() == 0 && o.count() == 0 && o.length() == 0);
    CHECK(o.keys().isEmpty() && o.begin() == o.end() && !o.contains("a"));
    CHECK(o.value("a").isUndefined() && std::as_const(o)["a"].isUndefined() && o.take("a").isUndefined());
    o.insert("b", 2);
    o.insert("a", 1);
    o.insert("c", "three");
    CHECK(o.size() == 3 && o.contains("a") && o.contains(QString("b")) && !o.contains("d"));
    CHECK(o.keys() == (QStringList{"a", "b", "c"}));
    CHECK(o.value("c") == "three" && o["a"] == 1 && o.value(QString("b")) == 2);
    CHECK(o.value(QLatin1StringView("a")) == 1 && o.value(QStringView(QString("a"))) == 1);
    CHECK(o.contains(QLatin1StringView("a")) && o.contains(QStringView(QString("a"))));

    auto replaced = o.insert("a", 100);
    CHECK(o.size() == 3 && o.value("a") == 100 && replaced.key() == "a" && replaced.value() == 100);
    auto none = o.insert("a", QJsonValue(QJsonValue::Undefined));
    CHECK(none == o.end() && !o.contains("a") && o.size() == 2);
    CHECK(o.insert("zz", QJsonValue(QJsonValue::Undefined)) == o.end() && o.size() == 2);

    o["x"] = 5;
    o["y"] = QJsonArray{1};
    CHECK(o.value("x") == 5 && o.value("y").isArray());
    o["x"] = "five";
    CHECK(o.value("x") == "five");
    CHECK(o["new"].isNull() && o.contains("new"));
    o.remove("new");
    CHECK(!o.contains("new"));
    o.remove("nothing");
    o.remove(QString("y"));
    o.remove(QLatin1StringView("x"));
    o.remove(QStringView(QString("zzz")));
    CHECK(o.keys() == (QStringList{"b", "c"}));
    QJsonValue taken = o.take("b");
    CHECK(taken == 2 && o.size() == 1);
    CHECK(o.take(QString("c")) == "three" && o.isEmpty());
    o.insert("k", 1);
    CHECK(o.take(QLatin1StringView("k")) == 1 && o.take(QStringView(QString("k"))).isUndefined());

    // Keys are sorted by UTF-16 code unit.
    QJsonObject sorted;
    for (const char *k : {"b", "B", "a", "ab", "", "é", "aa", "Z", "z"})
        sorted.insert(QString::fromUtf8(k), 1);
    CHECK(sorted.keys() == (QStringList{"", "B", "Z", "a", "aa", "ab", "b", "z", QString::fromUtf8("é")}));
    QJsonObject astral;
    astral.insert(QString::fromUtf8("\xEF\xBF\xBD"), 1);     // U+FFFD
    astral.insert(QString::fromUtf8("\xF0\x9F\x98\x80"), 1); // U+1F600, surrogates D83D DE00
    CHECK(astral.keys().at(0) == QString::fromUtf8("\xF0\x9F\x98\x80"));

    // Initializer list: last duplicate wins, undefined values are dropped.
    QJsonObject init{{"b", 1}, {"a", 2}, {"b", 3}, {"u", QJsonValue(QJsonValue::Undefined)}};
    CHECK(init.size() == 2 && init.value("b") == 3 && init.value("a") == 2 && !init.contains("u"));
    CHECK(QJsonObject{}.isEmpty());

    // Comparison does not depend on the insertion order.
    QJsonObject p, q;
    p.insert("a", 1);
    p.insert("b", QJsonArray{1});
    q.insert("b", QJsonArray{1});
    q.insert("a", 1.0);
    CHECK(p == q && !(p != q));
    q.insert("c", 1);
    CHECK(p != q);
    q.remove("c");
    CHECK(p == q);
    q["a"] = 2;
    CHECK(p != q && QJsonObject() == QJsonObject());
    QJsonObject gone{{"a", 1}};
    gone.remove("a");
    CHECK(gone == QJsonObject() && QJsonObject() == gone);
}

static void testObjectIterators() {
    QJsonObject o{{"c", 3}, {"a", 1}, {"b", 2}};
    std::string order;
    for (auto it = o.begin(); it != o.end(); ++it)
        order += it.key().toStdString() + std::to_string(it.value().toInt());
    CHECK(order == "a1b2c3");
    order.clear();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        order += it.key().toStdString() + std::to_string(it->toInt());
    CHECK(order == "a1b2c3");
    int sum = 0;
    for (const QJsonValue &v : o)
        sum += v.toInt();
    CHECK(sum == 6);
    for (QJsonValueRef v : o)
        v = v.toInt() * 10;
    CHECK(o.value("a") == 10 && o.value("b") == 20 && o.value("c") == 30);
    const QJsonObject &co = o;
    sum = 0;
    for (auto v : co)
        sum += v.toInt();
    CHECK(sum == 60);

    auto it = o.find("b");
    CHECK(it != o.end() && it.key() == "b" && it.value() == 20 && (*it).toInt() == 20 && it->toInt() == 20);
    CHECK(o.find(QString("zz")) == o.end() && o.find(QLatin1StringView("a")) != o.end());
    CHECK(o.find(QStringView(QString("c"))) != o.end());
    it.value() = 21;
    CHECK(o.value("b") == 21);
    *it = 22;
    CHECK(o.value("b") == 22);
    it->operator=(QJsonValue(23));
    CHECK(o.value("b") == 23);
    CHECK(it[1].toInt() == 30 && it[-1].toInt() == 10);

    auto cit = co.constFind("b");
    CHECK(cit != co.constEnd() && cit.key() == "b" && cit.value() == 23 && (*cit).toInt() == 23);
    CHECK(co.find("b") == cit && co.find(QString("b")) == cit && co.find(QLatin1StringView("b")) == cit);
    CHECK(co.find(QStringView(QString("b"))) == cit && co.constFind(QString("b")) == cit);
    CHECK(co.constFind(QLatin1StringView("b")) == cit && co.constFind(QStringView(QString("b"))) == cit);
    CHECK(co.constFind("zz") == co.constEnd() && co.find("zz") == co.end());

    // Arithmetic and comparisons.
    auto b = o.begin();
    CHECK(b + 3 == o.end() && o.end() - b == 3 && o.end() - 3 == b && 3 + b == o.end());
    CHECK((b++).key() == "a" && b.key() == "b" && (++b).key() == "c" && (b--).key() == "c" && (--b).key() == "a");
    b += 2;
    b -= 1;
    CHECK(b.key() == "b");
    CHECK(o.begin() < o.end() && o.begin() <= o.begin() && o.end() > o.begin() && o.end() >= o.end());
    QJsonObject::const_iterator cb = o.begin();
    CHECK(cb == o.constBegin() && o.begin() == cb && o.begin() == o.constBegin() && o.constBegin() == o.begin());
    CHECK(!(o.begin() != o.constBegin()) && o.begin() < o.constEnd() && o.begin() <= o.constEnd());
    CHECK(o.end() > o.constBegin() && o.end() >= o.constBegin() && o.constBegin() < o.end());
    CHECK((cb + 1).key() == "b" && (cb + 1 - cb) == 1 && (1 + cb).key() == "b" && (cb + 2 - 1).key() == "b");
    CHECK((cb++).key() == "a" && (++cb).key() == "c" && (cb--).key() == "c" && (--cb).key() == "a");
    cb += 1;
    cb -= 1;
    CHECK(cb[2].toInt() == 30 && cb < cb + 1 && cb <= cb && cb + 1 > cb && cb >= cb && cb != cb + 1);

    // Reverse iteration lists keys backwards.
    order.clear();
    for (auto r = o.rbegin(); r != o.rend(); ++r)
        order += std::to_string(r->toInt()) + ",";
    CHECK(order == "30,23,10,");
    order.clear();
    for (auto r = co.crbegin(); r != co.crend(); ++r)
        order += std::to_string((*r).toInt()) + ",";
    CHECK(order == "30,23,10,");

    // Erase.
    auto e = o.erase(o.find("b"));
    CHECK(e.key() == "c" && o.size() == 2 && !o.contains("b"));
    e = o.erase(o.find("c"));
    CHECK(e == o.end());
    e = o.erase(o.end());
    CHECK(e == o.end() && o.size() == 1);

    // Writing through an iterator does not disturb the others, and a copy keeps its own data.
    QJsonObject w{{"a", 1}, {"b", 2}};
    auto w0 = w.begin();
    auto w1 = w.begin() + 1;
    QJsonObject wcopy = w;
    *w0 = 10;
    *w1 = 20;
    CHECK(w.value("a") == 10 && w.value("b") == 20 && wcopy.value("a") == 1 && wcopy.value("b") == 2);
    CHECK(w0.key() == "a" && w1.key() == "b");

    // Inserting a key before the iterator's position moves the element it referred to.
    QJsonObject ins{{"m", 1}};
    auto im = ins.begin();
    ins.insert("a", 0);
    CHECK(im.key() == "a");
    CHECK(std::distance(ins.begin(), ins.end()) == 2);
}

static void testObjectSharing() {
    QJsonObject a{{"x", 1}};
    QJsonObject b = a;
    b.insert("y", 2);
    CHECK(a.size() == 1 && b.size() == 2);
    QJsonObject c = b;
    c["x"] = 100;
    CHECK(b.value("x") == 1 && c.value("x") == 100);
    QJsonObject d = c;
    d.remove("x");
    CHECK(c.contains("x") && !d.contains("x"));
    QJsonObject e = c;
    e.take("x");
    CHECK(c.contains("x"));
    QJsonValue v(a);
    a.insert("z", 3);
    CHECK(v.toObject().size() == 1);
    QJsonObject self{{"k", 1}};
    self.insert("self", self);
    CHECK(self.value("self").toObject().size() == 1 && self.size() == 2);
    QJsonObject moved = std::move(self);
    CHECK(moved.size() == 2 && self.isEmpty());
    self = moved;
    CHECK(self == moved);
    QJsonObject sw;
    sw.swap(self);
    CHECK(sw == moved && self.isEmpty());
}

static void testParseValid() {
    QJsonParseError::ParseError err;
    int off = -1;
    QJsonDocument d = parse(B("{}"), &err, &off);
    CHECK(d.isObject() && d.object().isEmpty() && err == QJsonParseError::NoError && off == 0);
    d = parse(B("[]"), &err, &off);
    CHECK(d.isArray() && d.array().isEmpty() && err == QJsonParseError::NoError);
    CHECK(!d.isNull() && !d.isEmpty());
    d = parse(B(" \t\r\n[ \n] \r\n\t "));
    CHECK(d.isArray());
    d = parse(B("[null, true, false, 0, -0, 1, -1, 1.5, -1.5e2, 1E3, 1e-3, 2e+2, \"s\", [], {}]"));
    CHECK(d.isArray() && d.array().size() == 15);
    const QJsonArray a = d.array();
    CHECK(a.at(0).isNull() && a.at(1) == true && a.at(2) == false && a.at(3) == 0 && a.at(4) == 0);
    CHECK(a.at(5) == 1 && a.at(6) == -1 && a.at(7).toDouble() == 1.5 && a.at(8).toDouble() == -150.0);
    CHECK(a.at(9).toDouble() == 1000.0 && a.at(10).toDouble() == 0.001 && a.at(11).toDouble() == 200.0);
    CHECK(a.at(12) == "s" && a.at(13).isArray() && a.at(14).isObject());
    CHECK(a.at(8).toInt() == -150 && a.at(9).toInteger() == 1000);

    d = parse(B("{\"b\": 1, \"a\": {\"x\": [1, 2, {\"y\": null}]}, \"c\" : \"\"}"));
    CHECK(d.isObject() && d.object().keys() == (QStringList{"a", "b", "c"}));
    CHECK(d["a"]["x"][2]["y"].isNull() && d["a"]["x"][1] == 2 && d["c"].isString() && d["c"].toString().isEmpty());
    CHECK(d["missing"].isUndefined() && d["a"]["x"][3].isUndefined());

    // The last of duplicate keys wins.
    d = parse(B("{\"a\": 1, \"b\": 2, \"a\": 3}"));
    CHECK(d.object().size() == 2 && d["a"] == 3);

    // A UTF-8 byte order mark is skipped.
    d = parse(B("\xEF\xBB\xBF{\"a\": 1}"));
    CHECK(d.isObject() && d["a"] == 1);

    // Numbers.
    d = parse(B("[9007199254740992, -9007199254740992, 9007199254740993, -9007199254740993, "
                "123456789012345678901234567890, 1.7976931348623157e308, 5e-324, 1e-400, -1e-400, 0.0, 0e0, "
                "100000000000000000000, 4.9e-324, 0.1, 12345678.125]"));
    CHECK(d.isArray());
    const QJsonArray n = d.array();
    CHECK(n.size() == 15);
    CHECK(n.at(0).toInteger() == (qint64(1) << 53) && n.at(1).toInteger() == -(qint64(1) << 53));
    CHECK(n.at(2).toDouble() == 9007199254740992.0 && n.at(3).toDouble() == -9007199254740992.0);
    CHECK(n.at(2).toInteger() == (qint64(1) << 53));
    CHECK(n.at(4).toDouble() == 1.2345678901234568e29);
    CHECK(n.at(5).toDouble() == std::numeric_limits<double>::max());
    CHECK(n.at(6).toDouble() == std::numeric_limits<double>::denorm_min());
    CHECK(n.at(7).toDouble() == 0.0 && n.at(8).toDouble() == 0.0 && n.at(9).toDouble() == 0.0 &&
          n.at(10).toDouble() == 0.0);
    CHECK(n.at(11).toDouble() == 1e20);
    CHECK(n.at(13).toDouble() == 0.1 && n.at(14).toDouble() == 12345678.125);
    d = parse(B("[1e-99999999999999999999, 0e99999999999999999999, 123e-400, 0.0000000000000000000000000000000"
                "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "00000000000000000000000000000000000000000000000000000000000000000000000001, 1e-320]"));
    CHECK(d.isArray() && d.array().size() == 5);
    CHECK(d.array().at(0).toDouble(1) == 0.0 && d.array().at(1).toDouble(1) == 0.0);
    CHECK(d.array().at(2).toDouble(1) == 0.0 && d.array().at(3).toDouble(1) == 0.0);
    CHECK(d.array().at(4).toDouble() > 0.0 && d.array().at(4).toDouble() < 1e-319);
    CHECK(parse(B("[1e1]")).array().at(0).toInt() == 10);
    CHECK(parse(B("[0.5e1]")).array().at(0).toInteger() == 5);
}

static void testParseStrings() {
    QJsonDocument d = parse(B("[\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"]"));
    CHECK(d.isArray() && d.array().at(0).toString() == QString::fromUtf8("\"\\/\b\f\n\r\t"));
    d = parse(B("[\"\\u0041\\u00e9\\u00E9\\u20ac\\uD83D\\uDE00\\u0000x\"]"));
    CHECK(d.isArray());
    const QString s = d.array().at(0).toString();
    CHECK(s.size() == 8);
    CHECK(s.at(0) == QChar(u'A') && s.at(1).unicode() == 0xE9 && s.at(2).unicode() == 0xE9);
    CHECK(s.at(3).unicode() == 0x20AC && s.at(4).unicode() == 0xD83D && s.at(5).unicode() == 0xDE00);
    CHECK(s.at(6).unicode() == 0 && s.at(7) == QChar(u'x'));
    d = parse(B("[\"h\xC3\xA9llo \xE2\x82\xAC \xF0\x9F\x98\x80 \xC2\x80 \xDF\xBF \xEF\xBF\xBF \xF4\x8F\xBF\xBF\"]"));
    CHECK(d.isArray());
    CHECK(d.array().at(0).toString() ==
          QString::fromUtf8(
              "h\xC3\xA9llo \xE2\x82\xAC \xF0\x9F\x98\x80 \xC2\x80 \xDF\xBF \xEF\xBF\xBF \xF4\x8F\xBF\xBF"));
    d = parse(B("{\"k\\u00e9y\": 1, \"\\ud83d\\ude00\": 2}"));
    CHECK(d["k\xC3\xA9y"] == 1 || d.object().value(QString::fromUtf8("k\xC3\xA9y")) == 1);
    CHECK(d.object().value(QString::fromUtf8("\xF0\x9F\x98\x80")) == 2);
    // Lone surrogates written as escapes are accepted.
    d = parse(B("[\"\\ud800\", \"\\udc00x\", \"\\ud800\\u0041\"]"));
    CHECK(d.isArray() && d.array().at(0).toString().size() == 1 && d.array().at(1).toString().size() == 2);
    CHECK(d.array().at(2).toString().size() == 2);
}

static void testParseErrors() {
    // The offset is the position of the parser when it gave up.
    CHECK(fails(B(""), QJsonParseError::IllegalValue, 0));
    CHECK(fails(B("\xEF\xBB\xBF"), QJsonParseError::IllegalValue, 3));
    CHECK(fails(B("\xEF\xBB"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("   "), QJsonParseError::IllegalValue, 3));
    CHECK(fails(B("1"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("\"s\""), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("null"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("x"), QJsonParseError::IllegalValue, 1));

    CHECK(fails(B("{"), QJsonParseError::UnterminatedObject, 1));
    CHECK(fails(B("{ \n  \n"), QJsonParseError::UnterminatedObject, 6));
    CHECK(fails(B("{\"a\": 1"), QJsonParseError::TerminationByNumber, 7));
    CHECK(fails(B("{\"a\": \"b\""), QJsonParseError::UnterminatedObject, 9));
    CHECK(fails(B("{\"a\": true"), QJsonParseError::UnterminatedObject, 10));
    CHECK(fails(B("{\"a\": true,"), QJsonParseError::UnterminatedObject, 11));
    CHECK(fails(B("{a: 1}"), QJsonParseError::UnterminatedObject, 2));
    CHECK(fails(B("{'a': 1}"), QJsonParseError::UnterminatedObject, 2));
    CHECK(fails(B("{1: 1}"), QJsonParseError::UnterminatedObject, 2));
    CHECK(fails(B("{\"a\" 1}"), QJsonParseError::MissingNameSeparator, 6));
    CHECK(fails(B("{\"a\"}"), QJsonParseError::MissingNameSeparator, 5));
    CHECK(fails(B("{\"a\"=1}"), QJsonParseError::MissingNameSeparator, 5));
    CHECK(fails(B("{\"a\":"), QJsonParseError::UnterminatedObject, 5));
    CHECK(fails(B("{\"a\": }"), QJsonParseError::IllegalValue, 6));
    CHECK(fails(B("{\"a\": 1 \"b\": 2}"), QJsonParseError::UnterminatedObject, 9));
    CHECK(fails(B("{\"a\": 1,}"), QJsonParseError::MissingObject, 9));
    CHECK(fails(B("{\"a\": 1, }"), QJsonParseError::MissingObject, 10));
    CHECK(fails(B("{,}"), QJsonParseError::UnterminatedObject, 2));

    CHECK(fails(B("["), QJsonParseError::UnterminatedArray, 1));
    CHECK(fails(B("[ \n"), QJsonParseError::UnterminatedArray, 3));
    CHECK(fails(B("[1"), QJsonParseError::TerminationByNumber, 2));
    CHECK(fails(B("[true"), QJsonParseError::UnterminatedArray, 5));
    CHECK(fails(B("[true,"), QJsonParseError::UnterminatedArray, 6));
    CHECK(fails(B("[true "), QJsonParseError::UnterminatedArray, 6));
    CHECK(fails(B("[1 2]"), QJsonParseError::MissingValueSeparator, 4));
    CHECK(fails(B("[true false]"), QJsonParseError::MissingValueSeparator, 7));
    CHECK(fails(B("[\"a\" \"b\"]"), QJsonParseError::MissingValueSeparator, 6));
    CHECK(fails(B("[1}"), QJsonParseError::UnterminatedArray, 3));
    CHECK(fails(B("[1,]"), QJsonParseError::MissingObject, 4));
    CHECK(fails(B("[,]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[1,,2]"), QJsonParseError::IllegalValue, 3));
    CHECK(fails(B("[x]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[tru]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[nul]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[fals]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[True]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[NaN]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[Infinity]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[+1]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[.5]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[// c\n1]"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("[1] [2]"), QJsonParseError::GarbageAtEnd, 4));

    // Numbers.
    CHECK(fails(B("[-]"), QJsonParseError::IllegalNumber, 2));
    CHECK(fails(B("[-"), QJsonParseError::TerminationByNumber, 2));
    CHECK(fails(B("[-a]"), QJsonParseError::IllegalNumber, 2));
    CHECK(fails(B("[1.]"), QJsonParseError::IllegalNumber, 3));
    CHECK(fails(B("[1.e5]"), QJsonParseError::IllegalNumber, 3));
    CHECK(fails(B("[1e]"), QJsonParseError::IllegalNumber, 3));
    CHECK(fails(B("[1e+]"), QJsonParseError::IllegalNumber, 4));
    CHECK(fails(B("[1E-x]"), QJsonParseError::IllegalNumber, 4));
    CHECK(fails(B("[1e999]"), QJsonParseError::IllegalNumber, 6));
    CHECK(fails(B("[1e99999999999999999999]"), QJsonParseError::IllegalNumber, 23));
    CHECK(fails(B("[1") + QByteArray(400, '0') + B("]"), QJsonParseError::IllegalNumber, 402));
    CHECK(fails(B("[-1e999]"), QJsonParseError::IllegalNumber, 7));
    CHECK(fails(B("[0x10]"), QJsonParseError::MissingValueSeparator, 3));
    CHECK(fails(B("[01]"), QJsonParseError::MissingValueSeparator, 3));
    CHECK(fails(B("[1.5.5]"), QJsonParseError::MissingValueSeparator, 5));
    CHECK(fails(B("[--1]"), QJsonParseError::IllegalNumber, 2));
    CHECK(fails(B("[1.5"), QJsonParseError::TerminationByNumber, 4));
    CHECK(fails(B("[1e5"), QJsonParseError::TerminationByNumber, 4));

    // Strings.
    CHECK(fails(B("[\"abc"), QJsonParseError::UnterminatedString, 5));
    CHECK(fails(B("[\""), QJsonParseError::UnterminatedString, 2));
    CHECK(fails(B("{\"abc"), QJsonParseError::UnterminatedString, 5));
    CHECK(fails(B("[\"a\\"), QJsonParseError::UnterminatedString, 4));
    CHECK(fails(B("[\"\\x\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"\\'\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"\\u12\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"\\u12G4\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"\\u\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"\\U0041\"]"), QJsonParseError::IllegalEscapeSequence, 4));
    CHECK(fails(B("[\"a\nb\"]"), QJsonParseError::IllegalValue, 3));
    CHECK(fails(B("[\"a\tb\"]"), QJsonParseError::IllegalValue, 3));
    CHECK(fails(QByteArray("[\"a\0b\"]", 7), QJsonParseError::IllegalValue, 3));
    CHECK(fails(B("[\"\x1F\"]"), QJsonParseError::IllegalValue, 2));

    // Invalid UTF-8.
    CHECK(fails(B("[\"\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xBF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xC0\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xC1\xBF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xC3\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xC3("), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xE0\x80\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xE0\x9F\xBF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xED\xA0\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xED\xBF\xBF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xF0\x80\x80\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xF0\x8F\xBF\xBF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xF4\x90\x80\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xF5\x80\x80\x80\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xFE\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xFF\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xE2\x82\"]"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("[\"\xF0\x9F\x98"), QJsonParseError::IllegalUTF8String, 2));
    CHECK(fails(B("{\"\xC3\": 1}"), QJsonParseError::IllegalUTF8String, 2));

    // Garbage after the document.
    CHECK(fails(B("{} x"), QJsonParseError::GarbageAtEnd, 3));
    CHECK(fails(B("[] []"), QJsonParseError::GarbageAtEnd, 3));
    CHECK(fails(B("{}{}"), QJsonParseError::GarbageAtEnd, 2));
    CHECK(fails(B("[1]\n,"), QJsonParseError::GarbageAtEnd, 4));
    CHECK(fails(B("[]\x01"), QJsonParseError::GarbageAtEnd, 2));
    CHECK(fails(B("[]\xEF\xBB\xBF"), QJsonParseError::GarbageAtEnd, 2));
    CHECK(fails(QByteArray("[]\0", 3), QJsonParseError::GarbageAtEnd, 2));

    // Mismatched brackets.
    CHECK(fails(B("[}"), QJsonParseError::IllegalValue, 1));
    CHECK(fails(B("{]"), QJsonParseError::UnterminatedObject, 2));
    CHECK(fails(B("[{]"), QJsonParseError::UnterminatedObject, 3));
    CHECK(fails(B("{\"a\": [}"), QJsonParseError::IllegalValue, 7));

    // A null error pointer is fine, and fromJson fills in NoError on success.
    CHECK(QJsonDocument::fromJson(B("[1")).isNull());
    QJsonParseError e;
    e.error = QJsonParseError::GarbageAtEnd;
    e.offset = 3;
    CHECK(!QJsonDocument::fromJson(B("[1]"), &e).isNull() && e.error == QJsonParseError::NoError && e.offset == 0);
}

static void testErrorStrings() {
    struct Row {
        QJsonParseError::ParseError e;
        const char *text;
    };
    const Row rows[] = {
        {QJsonParseError::NoError, "no error occurred"},
        {QJsonParseError::UnterminatedObject, "unterminated object"},
        {QJsonParseError::MissingNameSeparator, "missing name separator"},
        {QJsonParseError::UnterminatedArray, "unterminated array"},
        {QJsonParseError::MissingValueSeparator, "missing value separator"},
        {QJsonParseError::IllegalValue, "illegal value"},
        {QJsonParseError::TerminationByNumber, "invalid termination by number"},
        {QJsonParseError::IllegalNumber, "illegal number"},
        {QJsonParseError::IllegalEscapeSequence, "invalid escape sequence"},
        {QJsonParseError::IllegalUTF8String, "invalid UTF8 string"},
        {QJsonParseError::UnterminatedString, "unterminated string"},
        {QJsonParseError::MissingObject, "object is missing after a comma"},
        {QJsonParseError::DeepNesting, "too deeply nested document"},
        {QJsonParseError::DocumentTooLarge, "too large document"},
        {QJsonParseError::GarbageAtEnd, "garbage at the end of the document"},
    };
    int value = 0;
    for (const Row &r : rows) {
        QJsonParseError e;
        e.error = r.e;
        CHECK(int(r.e) == value++);
        CHECK(e.errorString() == QString::fromUtf8(r.text));
    }
    QJsonParseError fresh;
    CHECK(fresh.error == QJsonParseError::NoError && fresh.offset == 0);
}

static void testNesting() {
    auto nestedArrays = [](int depth) { return QByteArray(depth, '[') + QByteArray(depth, ']'); };
    QJsonDocument d = parse(nestedArrays(1024));
    CHECK(d.isArray());
    QJsonValue v = d.array();
    int depth = 0;
    while (v.isArray() && v.toArray().size() == 1) {
        v = v.toArray().at(0);
        ++depth;
    }
    CHECK(depth == 1023 && v.isArray() && v.toArray().isEmpty());
    CHECK(fails(nestedArrays(1025), QJsonParseError::DeepNesting, 1025));
    CHECK(fails(nestedArrays(5000), QJsonParseError::DeepNesting, 1025));

    QByteArray objects;
    for (int i = 0; i < 1024; ++i)
        objects += "{\"a\":";
    objects += "1";
    objects += QByteArray(1024, '}');
    d = parse(objects);
    CHECK(d.isObject());
    QJsonObject deep;
    d = parse(QByteArray("{\"a\":") + objects + "}");
    CHECK(d.isNull());
    QJsonParseError::ParseError err;
    int off;
    parse(QByteArray("{\"a\":") + objects + "}", &err, &off);
    CHECK(err == QJsonParseError::DeepNesting);

    // Mixed nesting counts both kinds.
    QByteArray mixed;
    for (int i = 0; i < 512; ++i)
        mixed += "[{\"a\":";
    mixed += "1";
    for (int i = 0; i < 512; ++i)
        mixed += "}]";
    CHECK(parse(mixed).isArray());
    parse(QByteArray("[") + mixed + "]", &err, &off);
    CHECK(err == QJsonParseError::DeepNesting);

    // Wide documents are not nested.
    QByteArray wide = "[";
    for (int i = 0; i < 5000; ++i)
        wide += (i ? ",[]" : "[]");
    wide += "]";
    CHECK(parse(wide).array().size() == 5000);

    // Nesting made by hand writes and compares fine.
    QJsonArray inner;
    for (int i = 0; i < 100; ++i) {
        QJsonArray outer;
        outer.append(inner);
        inner = outer;
    }
    QJsonDocument hand(inner);
    CHECK(QJsonDocument::fromJson(hand.toJson(QJsonDocument::Compact)) == hand);
}

static void testLarge() {
    QJsonArray big;
    for (int i = 0; i < 100000; ++i) {
        QJsonObject o;
        o["id"] = i;
        o["name"] = QString("item%1").arg(i);
        o["tags"] = QJsonArray{"a", "b", i % 7};
        o["ratio"] = i / 8.0;
        o["on"] = (i % 2) == 0;
        big.append(o);
    }
    const QJsonDocument doc(big);
    const QByteArray compact = doc.toJson(QJsonDocument::Compact);
    const QByteArray indented = doc.toJson(QJsonDocument::Indented);
    CHECK(compact.size() > 5000000);
    QJsonParseError err;
    const QJsonDocument back = QJsonDocument::fromJson(compact, &err);
    CHECK(err.error == QJsonParseError::NoError && back == doc);
    CHECK(QJsonDocument::fromJson(indented) == doc);
    CHECK(back.array().size() == 100000 && back.array().at(99999)["id"] == 99999);
    CHECK(back.array().at(1234)["ratio"].toDouble() == 1234 / 8.0);

    // One object with very many keys, inserted in an unsorted order.
    QJsonObject many;
    for (int i = 0; i < 50000; ++i)
        many.insert(QString::number((i * 7919) % 50021), i);
    CHECK(many.size() == 50000);
    const QStringList keys = many.keys();
    CHECK(std::is_sorted(keys.begin(), keys.end()));
    QByteArray text = QJsonDocument(many).toJson(QJsonDocument::Compact);
    CHECK(QJsonDocument::fromJson(text).object() == many);

    // A long string.
    QString longString(2000000, QChar(u'x'));
    longString += QString::fromUtf8("\xF0\x9F\x98\x80");
    QJsonArray one{longString};
    CHECK(QJsonDocument::fromJson(QJsonDocument(one).toJson()).array().at(0).toString() == longString);
}

static void testWriter() {
    QJsonObject o;
    o["Key"] = "Value";
    o["Array"] = QJsonArray{true, 999};
    QJsonDocument doc(o);
    CHECK(doc.toJson(QJsonDocument::Compact) == B("{\"Array\":[true,999],\"Key\":\"Value\"}"));
    CHECK(doc.toJson() == B("{\n    \"Array\": [\n        true,\n        999\n    ],\n    \"Key\": \"Value\"\n}\n"));
    CHECK(doc.toJson(QJsonDocument::Indented) == doc.toJson());

    // Empty containers.
    CHECK(QJsonDocument(QJsonObject()).toJson(QJsonDocument::Compact) == B("{}"));
    CHECK(QJsonDocument(QJsonArray()).toJson(QJsonDocument::Compact) == B("[]"));
    CHECK(QJsonDocument(QJsonObject()).toJson() == B("{\n}\n"));
    CHECK(QJsonDocument(QJsonArray()).toJson() == B("[\n]\n"));
    QJsonObject nested;
    nested["e"] = QJsonObject();
    nested["a"] = QJsonArray();
    nested["n"] = QJsonValue();
    CHECK(QJsonDocument(nested).toJson() == B("{\n    \"a\": [\n    ],\n    \"e\": {\n    },\n    \"n\": null\n}\n"));
    CHECK(QJsonDocument(nested).toJson(QJsonDocument::Compact) == B("{\"a\":[],\"e\":{},\"n\":null}"));
    QJsonArray deep{QJsonArray{QJsonArray{1}}, QJsonObject{{"k", QJsonArray{}}}};
    CHECK(QJsonDocument(deep).toJson() ==
          B("[\n    [\n        [\n            1\n        ]\n    ],\n    {\n        \"k\": [\n        ]\n    }\n]\n"));

    // A null document writes nothing.
    CHECK(QJsonDocument().toJson().isEmpty() && QJsonDocument().toJson(QJsonDocument::Compact).isEmpty());

    // Escapes.
    QJsonArray s;
    s.append(QString::fromUtf8("\"\\/\b\f\n\r\t"));
    s.append(QString::fromUtf8("\x01\x1f\x7f"));
    s.append(QString::fromUtf8("h\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 \xE2\x80\xA8"));
    s.append(QString(QChar(0)));
    QString lone;
    lone += QChar(0xD800);
    lone += QChar(u'x');
    lone += QChar(0xDC00);
    s.append(lone);
    CHECK(QJsonDocument(s).toJson(QJsonDocument::Compact) ==
          B("[\"\\\"\\\\/\\b\\f\\n\\r\\t\",\"\\u0001\\u001f\x7f\",\"h\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 "
            "\xE2\x80\xA8\","
            "\"\\u0000\",\"\xEF\xBF\xBDx\xEF\xBF\xBD\"]"));
    QJsonObject keyed;
    keyed.insert(QString::fromUtf8("k\"\n\xC3\xA9"), 1);
    CHECK(QJsonDocument(keyed).toJson(QJsonDocument::Compact) == B("{\"k\\\"\\n\xC3\xA9\":1}"));

    // Numbers.
    auto num = [](QJsonValue v) {
        return QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
    };
    CHECK(num(0) == B("0") && num(-0.0) == B("0") && num(1) == B("1") && num(-1) == B("-1"));
    CHECK(num(1.0) == B("1") && num(-5.0) == B("-5") && num(1e15) == B("1000000000000000"));
    CHECK(num(qint64(9007199254740992LL)) == B("9007199254740992"));
    CHECK(num(qint64(-9007199254740992LL)) == B("-9007199254740992"));
    CHECK(num(9007199254740992.0) == B("9007199254740992"));
    CHECK(num(9007199254740994.0) == B("9007199254740994"));
    CHECK(num(qint64(9007199254740993LL)) == B("9007199254740992"));
    CHECK(num(1e16) == B("1e+16") && num(1e20) == B("1e+20") && num(1.5e300) == B("1.5e+300"));
    CHECK(num(1.2345678901234567e19) == B("1.2345678901234567e+19"));
    CHECK(num(0.5) == B("0.5") && num(-0.5) == B("-0.5") && num(0.1) == B("0.1") && num(1.5) == B("1.5"));
    CHECK(num(3.14159) == B("3.14159") && num(123456789.125) == B("123456789.125"));
    CHECK(num(0.0001) == B("0.0001") && num(0.00012) == B("0.00012") && num(0.00001) == B("1e-05"));
    CHECK(num(1.5e-7) == B("1.5e-07") && num(1e-100) == B("1e-100") && num(-2.5e-10) == B("-2.5e-10"));
    CHECK(num(5e-324) == B("5e-324") && num(1.7976931348623157e308) == B("1.7976931348623157e+308"));
    CHECK(num(0.30000000000000004) == B("0.30000000000000004") && num(100.25) == B("100.25"));
    CHECK(num(std::numeric_limits<double>::quiet_NaN()) == B("null"));
    CHECK(num(std::numeric_limits<double>::infinity()) == B("null"));
    CHECK(num(QJsonValue(QJsonValue::Undefined)) == B("null"));
    CHECK(num(true) == B("true") && num(false) == B("false"));

    // Every double written reads back as the same value.
    const double samples[] = {0.1,
                              0.2,
                              0.3,
                              1.0 / 3.0,
                              2.0 / 3.0,
                              1e-5,
                              123456.789e3,
                              6.02214076e23,
                              1.602176634e-19,
                              9007199254740993.0,
                              4503599627370497.5,
                              2.2250738585072014e-308,
                              1e22,
                              1e23};
    for (double sample : samples) {
        const QJsonDocument one(QJsonArray{sample});
        CHECK(QJsonDocument::fromJson(one.toJson()).array().at(0).toDouble() == sample);
    }
}

static void testRoundTrip() {
    const char *docs[] = {
        "{}",
        "[]",
        "[1,2,3]",
        "{\"a\":1,\"b\":[true,false,null],\"c\":{\"d\":\"e\"}}",
        "[\"\\u0001\\\"\\\\\\n\\t\",\"\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\"]",
        "[0,-1,1.5,1e+20,0.0001,1e-05,9007199254740992,123456789.125]",
        "{\"a\":{\"b\":{\"c\":{\"d\":[[],[[]],{}]}}}}",
        "[{\"\":1},{\" \":2},{\"\\u0000\":3}]",
    };
    for (const char *text : docs) {
        const QByteArray in(text);
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(in, &err);
        CHECK(err.error == QJsonParseError::NoError);
        CHECK(doc.toJson(QJsonDocument::Compact) == in);
        const QJsonDocument again = QJsonDocument::fromJson(doc.toJson(QJsonDocument::Indented));
        CHECK(again == doc);
        CHECK(again.toJson(QJsonDocument::Compact) == in);
    }

    // Unsorted keys are written sorted.
    const QJsonDocument d = QJsonDocument::fromJson(B("{\"b\":1,\"a\":2,\"c\":{\"z\":1,\"y\":2}}"));
    CHECK(d.toJson(QJsonDocument::Compact) == B("{\"a\":2,\"b\":1,\"c\":{\"y\":2,\"z\":1}}"));

    // Whitespace, number spelling and escapes normalise.
    const QJsonDocument n = QJsonDocument::fromJson(B(" [ 1.0 , 1E2 , -0 , \"\\u0041\\/\" ] "));
    CHECK(n.toJson(QJsonDocument::Compact) == B("[1,100,0,\"A/\"]"));
}

static void testDocument() {
    QJsonDocument null;
    CHECK(null.isNull() && null.isEmpty() && !null.isArray() && !null.isObject());
    CHECK(null.object().isEmpty() && null.array().isEmpty());
    CHECK(null["a"].isUndefined() && null[0].isUndefined());
    CHECK(null == QJsonDocument() && !(null != QJsonDocument()));

    QJsonDocument od{QJsonObject{{"a", 1}}};
    CHECK(!od.isNull() && !od.isEmpty() && od.isObject() && !od.isArray());
    CHECK(od.object().size() == 1 && od.array().isEmpty());
    CHECK(od["a"] == 1 && od[QString("a")] == 1 && od[QLatin1StringView("a")] == 1);
    CHECK(od[QStringView(QString("a"))] == 1 && od["b"].isUndefined() && od[0].isUndefined());

    QJsonDocument ad{QJsonArray{1, 2}};
    CHECK(!ad.isNull() && ad.isArray() && !ad.isObject() && ad.array().size() == 2 && ad.object().isEmpty());
    CHECK(ad[1] == 2 && ad[2].isUndefined() && ad["a"].isUndefined());

    // Empty containers still make a document that is not null.
    CHECK(!QJsonDocument(QJsonObject()).isNull() && QJsonDocument(QJsonObject()).isObject());
    CHECK(!QJsonDocument(QJsonArray()).isNull() && QJsonDocument(QJsonArray()).isArray());
    CHECK(QJsonDocument(QJsonObject()) != QJsonDocument(QJsonArray()));
    CHECK(QJsonDocument(QJsonObject()) != QJsonDocument());

    QJsonDocument d;
    d.setObject(QJsonObject{{"x", 1}});
    CHECK(d.isObject() && d["x"] == 1);
    d.setArray(QJsonArray{5});
    CHECK(d.isArray() && d[0] == 5 && !d.isObject());
    d.setObject(QJsonObject());
    CHECK(d.isObject() && d.object().isEmpty());

    QJsonDocument copy = od;
    CHECK(copy == od);
    copy.setObject(QJsonObject{{"a", 2}});
    CHECK(copy != od && od["a"] == 1);
    QJsonDocument moved = std::move(copy);
    CHECK(moved["a"] == 2 && copy.isNull());
    QJsonDocument assigned;
    assigned = moved;
    CHECK(assigned == moved);
    QJsonDocument movedAssigned;
    movedAssigned = std::move(assigned);
    CHECK(movedAssigned == moved);
    movedAssigned.swap(od);
    CHECK(od == moved && movedAssigned["a"] == 1);

    // Equality compares contents, not spelling.
    CHECK(QJsonDocument::fromJson(B("{\"a\":1,\"b\":2}")) == QJsonDocument::fromJson(B("{ \"b\" : 2.0, \"a\" : 1 }")));
    CHECK(QJsonDocument::fromJson(B("[1,2]")) != QJsonDocument::fromJson(B("[2,1]")));
    CHECK(QJsonDocument::fromJson(B("[1,2]")) != QJsonDocument::fromJson(B("[1,2,3]")));

    // The result of a failed parse is null and not empty-array.
    CHECK(QJsonDocument::fromJson(B("[")).isNull() && QJsonDocument::fromJson(B("[")).toJson().isEmpty());
}

static QList<QString> g_messages;
static void captureHandler(QtMsgType, const QMessageLogContext &, const QString &msg) { g_messages.append(msg); }

static void checkMessage(int index, const char *expected) {
    const QString actual = g_messages.value(index);
    if (actual != QString::fromUtf8(expected)) {
        std::printf("FAIL debug message %d: '%s' (expected '%s')\n", index, actual.toStdString().c_str(), expected);
        ++g_failures;
    }
}

static void testDebug() {
    qInstallMessageHandler(captureHandler);
    qDebug() << QJsonValue();
    qDebug() << QJsonValue(QJsonValue::Undefined);
    qDebug() << QJsonValue(true);
    qDebug() << QJsonValue(1.5);
    qDebug() << QJsonValue("s");
    qDebug() << QJsonValue(QJsonArray{1, 2});
    qDebug() << QJsonValue(QJsonObject{{"a", 1}});
    qDebug() << QJsonArray{1, "x", QJsonObject{{"k", QJsonArray{}}}};
    qDebug() << QJsonArray();
    qDebug() << QJsonObject{{"b", 1}, {"a", QJsonArray{true}}};
    qDebug() << QJsonObject();
    qDebug() << QJsonDocument(QJsonObject{{"a", 1}});
    qDebug() << QJsonDocument(QJsonArray{1});
    qDebug() << "x" << QJsonArray{1} << QJsonValue(2) << 3;
    qInstallMessageHandler(nullptr);
    CHECK(g_messages.size() == 14);
    checkMessage(0, "QJsonValue(null)");
    checkMessage(1, "QJsonValue(undefined)");
    checkMessage(2, "QJsonValue(bool, true)");
    checkMessage(3, "QJsonValue(double, 1.5)");
    checkMessage(4, "QJsonValue(string, \"s\")");
    checkMessage(5, "QJsonValue(array, QJsonArray([1,2]))");
    checkMessage(6, "QJsonValue(object, QJsonObject({\"a\":1}))");
    checkMessage(7, "QJsonArray([1,\"x\",{\"k\":[]}])");
    checkMessage(8, "QJsonArray([])");
    checkMessage(9, "QJsonObject({\"a\":[true],\"b\":1})");
    checkMessage(10, "QJsonObject({})");
    checkMessage(11, "QJsonDocument(QJsonObject({\"a\":1}))");
    checkMessage(12, "QJsonDocument(QJsonArray([1]))");
    checkMessage(13, "x QJsonArray([1]) QJsonValue(double, 2) 3");
}

static void testTypes() {
    static_assert(std::is_copy_constructible_v<QJsonValue> && std::is_nothrow_move_constructible_v<QJsonValue>);
    static_assert(std::is_nothrow_move_constructible_v<QJsonArray> && std::is_nothrow_move_assignable_v<QJsonArray>);
    static_assert(std::is_nothrow_move_constructible_v<QJsonObject> && std::is_nothrow_move_assignable_v<QJsonObject>);
    static_assert(std::is_nothrow_move_constructible_v<QJsonDocument>);
    static_assert(std::is_base_of_v<QJsonValueConstRef, QJsonValueRef>);
    static_assert(std::is_same_v<QJsonArray::value_type, QJsonValue>);
    static_assert(std::is_same_v<QJsonObject::key_type, QString>);
    static_assert(std::is_convertible_v<QJsonValueRef, QJsonValue>);
    static_assert(std::is_convertible_v<QJsonValueConstRef, QJsonValue>);
    static_assert(std::is_convertible_v<QJsonArray::iterator, QJsonArray::const_iterator>);
    static_assert(std::is_convertible_v<QJsonObject::iterator, QJsonObject::const_iterator>);
    static_assert(
        std::is_same_v<std::iterator_traits<QJsonArray::iterator>::iterator_category, std::random_access_iterator_tag>);
    CHECK(QJsonParseError::NoError == 0);
    CHECK(QJsonValue::Undefined == 0x80 && QJsonValue::Object == 5);
    CHECK(QJsonDocument::Indented == 0 && QJsonDocument::Compact == 1);
}

static void testVariantScalars() {
    using V = QJsonValue;
    CHECK(V::fromVariant(QVariant()).isNull());
    CHECK(V::fromVariant(QVariant(nullptr)).isNull());
    CHECK(V::fromVariant(QVariant(true)).isBool() && V::fromVariant(QVariant(true)).toBool());
    CHECK(V::fromVariant(QVariant(false)).isBool());
    CHECK(V::fromVariant(QVariant(42)).isDouble() && V::fromVariant(QVariant(42)).toInt() == 42);
    CHECK(V::fromVariant(QVariant(-7)).toInteger() == -7);
    CHECK(V::fromVariant(QVariant(4000000000u)).toInteger() == 4000000000LL);
    CHECK(V::fromVariant(QVariant::fromValue<short>(-3)).toInt() == -3);
    CHECK(V::fromVariant(QVariant::fromValue<unsigned short>(65535)).toInt() == 65535);
    CHECK(V::fromVariant(QVariant::fromValue<long>(123456789012L)).toInteger() == 123456789012L);
    CHECK(V::fromVariant(QVariant(qlonglong(1) << 53)).toInteger() == (qlonglong(1) << 53));
    CHECK(V::fromVariant(QVariant(qulonglong(9007199254740992ULL))).toInteger() == 9007199254740992LL);
    CHECK(V::fromVariant(QVariant(std::numeric_limits<qulonglong>::max())).toDouble() == 18446744073709551616.0);
    CHECK(V::fromVariant(QVariant(2.5)).toDouble() == 2.5);
    CHECK(V::fromVariant(QVariant(1.5f)).toDouble() == 1.5);
    CHECK(V::fromVariant(QVariant(std::nan(""))).isNull());
    CHECK(V::fromVariant(QVariant(std::numeric_limits<double>::infinity())).isNull());
    CHECK(V::fromVariant(QVariant(QString("hi"))).toString() == "hi");
    CHECK(V::fromVariant(QVariant(QString())).isString());
    CHECK(V::fromVariant(QVariant(QByteArray("bytes"))).toString() == "bytes");
    CHECK(V::fromVariant(QVariant(QByteArray())).isNull());
    CHECK(V::fromVariant(QVariant(QChar('x'))).toString() == "x");
    CHECK(V::fromVariant(QVariant(QUrl("http://example.com/a b?q=1"))).toString() ==
          QString::fromLatin1(QUrl("http://example.com/a b?q=1").toEncoded()));
    const QUuid id = QUuid::fromString("{67C8770B-44F1-410A-AB9A-F9B5446F13EE}");
    CHECK(V::fromVariant(QVariant::fromValue(id)).toString() == "67c8770b-44f1-410a-ab9a-f9b5446f13ee");
    CHECK(V::fromVariant(QVariant(QDate(2024, 2, 29))).toString() == "2024-02-29");
    CHECK(V::fromVariant(QVariant(QDate())).isNull());
    const QDateTime dt(QDate(2024, 2, 29), QTime(13, 45, 6), Qt::UTC);
    CHECK(V::fromVariant(QVariant(dt)).toString() == dt.toString(Qt::ISODateWithMs));
    struct Opaque {};
    CHECK(V::fromVariant(QVariant::fromValue(Opaque{})).isNull());
}

static void testVariantContainers() {
    const QVariantList list{1, "two", 3.5, true, QVariant(), QStringList{"a", "b"}};
    const QJsonArray arr = QJsonArray::fromVariantList(list);
    CHECK(arr.size() == 6);
    CHECK(arr[0].toInt() == 1 && arr[1].toString() == "two" && arr[2].toDouble() == 3.5);
    CHECK(arr[3].toBool() && arr[4].isNull());
    CHECK(arr[5].isArray() && arr[5].toArray().size() == 2 && arr[5].toArray()[1].toString() == "b");
    CHECK(QJsonValue::fromVariant(QVariant(QStringList{"x"})).toArray().at(0).toString() == "x");
    CHECK(QJsonValue::fromVariant(QVariant(list)).isArray());
    CHECK(QJsonArray::fromVariantList(QVariantList()).isEmpty());

    QVariantMap map;
    map["name"] = "nt";
    map["count"] = 3;
    map["nested"] = QVariantMap{{"ok", true}, {"list", QVariantList{1, 2}}};
    const QJsonObject obj = QJsonObject::fromVariantMap(map);
    CHECK(obj.size() == 3 && obj["name"].toString() == "nt" && obj["count"].toInt() == 3);
    CHECK(obj["nested"]["ok"].toBool() && obj["nested"]["list"][1].toInt() == 2);
    CHECK(QJsonValue::fromVariant(QVariant(map)).isObject());

    QVariantHash hash;
    hash["b"] = 2;
    hash["a"] = QVariant();
    const QJsonObject hobj = QJsonObject::fromVariantHash(hash);
    CHECK(hobj.size() == 2 && hobj["a"].isNull() && hobj["b"].toInt() == 2);
    CHECK(hobj.keys() == QStringList({"a", "b"}));
    CHECK(QJsonValue::fromVariant(QVariant(hash)).isObject());
    CHECK(QJsonObject::fromVariantMap(QVariantMap()).isEmpty());

    CHECK(QJsonValue::fromVariant(QVariant::fromValue(QJsonValue(5))).toInt() == 5);
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(QJsonValue(QJsonValue::Undefined))).isUndefined());
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(arr)).isArray());
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(obj)).isObject());
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(QJsonDocument(arr))).isArray());
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(QJsonDocument(obj))).isObject());
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(QJsonDocument())).isObject());
}

static void testVariantToVariant() {
    CHECK(QJsonValue(QJsonValue::Null).toVariant().typeId() == QMetaType::Nullptr);
    CHECK(QJsonValue(QJsonValue::Null).toVariant().isNull());
    CHECK(!QJsonValue(QJsonValue::Undefined).toVariant().isValid());
    CHECK(QJsonValue(true).toVariant().typeId() == QMetaType::Bool && QJsonValue(true).toVariant().toBool());
    CHECK(QJsonValue(5).toVariant().typeId() == QMetaType::LongLong && QJsonValue(5).toVariant().toInt() == 5);
    CHECK(QJsonValue(2.5).toVariant().typeId() == QMetaType::Double && QJsonValue(2.5).toVariant().toDouble() == 2.5);
    CHECK(QJsonValue(qint64(1) << 60).toVariant().typeId() == QMetaType::Double);
    CHECK(QJsonValue("s").toVariant().typeId() == QMetaType::QString && QJsonValue("s").toVariant().toString() == "s");
    CHECK(QJsonValue(QJsonArray{1}).toVariant().typeId() == QMetaType::QVariantList);
    CHECK(QJsonValue(QJsonObject{{"a", 1}}).toVariant().typeId() == QMetaType::QVariantMap);

    const QJsonObject obj{
        {"s", "str"}, {"n", QJsonValue()}, {"i", 7}, {"d", 0.25}, {"a", QJsonArray{1, "x", QJsonObject{{"k", false}}}}};
    const QVariantMap map = obj.toVariantMap();
    CHECK(map.size() == 5 && map["s"].toString() == "str" && map["i"].toInt() == 7 && map["d"].toDouble() == 0.25);
    CHECK(map["n"].typeId() == QMetaType::Nullptr);
    const QVariantList inner = map["a"].toList();
    CHECK(inner.size() == 3 && inner[0].toInt() == 1 && inner[1].toString() == "x");
    CHECK(inner[2].toMap()["k"].toBool() == false && inner[2].toMap().contains("k"));
    const QVariantHash hash = obj.toVariantHash();
    CHECK(hash.size() == 5 && hash["i"].toInt() == 7 && hash["a"].toList().size() == 3);
    CHECK(QJsonObject().toVariantMap().isEmpty() && QJsonObject().toVariantHash().isEmpty());
    CHECK(QJsonArray().toVariantList().isEmpty());
    CHECK(QJsonArray({1, 2, 3}).toVariantList() == QVariantList({1, 2, 3}));
}

static void testVariantRoundTrip() {
    QVariantMap map;
    map["null"] = QVariant::fromValue(nullptr);
    map["bool"] = true;
    map["int"] = 12345;
    map["big"] = qlonglong(9007199254740992LL);
    map["neg"] = qlonglong(-9007199254740992LL);
    map["double"] = 0.125;
    map["string"] = "text \xE2\x82\xAC";
    map["list"] = QVariantList{1, QVariantMap{{"x", "y"}}, QVariantList()};
    map["map"] = QVariantMap{{"inner", QVariantList{false}}};
    const QJsonObject obj = QJsonObject::fromVariantMap(map);
    const QVariantMap back = obj.toVariantMap();
    CHECK(back.keys() == map.keys());
    CHECK(back["bool"].toBool() && back["int"].toInt() == 12345);
    CHECK(back["big"].toLongLong() == 9007199254740992LL && back["neg"].toLongLong() == -9007199254740992LL);
    CHECK(back["double"].toDouble() == 0.125 && back["string"].toString() == map["string"].toString());
    CHECK(back == map);
    CHECK(QJsonObject::fromVariantMap(back) == obj);

    // Through text as well.
    QJsonParseError err;
    const QJsonDocument parsed = QJsonDocument::fromJson(QJsonDocument(obj).toJson(), &err);
    CHECK(err.error == QJsonParseError::NoError && parsed.object() == obj);
    CHECK(parsed.object().toVariantMap() == back);

    const QVariantList list{1, "a", 2.5, true, QVariant::fromValue(nullptr), QVariantList{3}};
    CHECK(QJsonArray::fromVariantList(list).toVariantList() == list);
    CHECK(QJsonValue::fromVariant(QJsonValue(QJsonArray{1, "z"}).toVariant()) == QJsonValue(QJsonArray{1, "z"}));
    for (const QJsonValue &v : {QJsonValue(), QJsonValue(true), QJsonValue(0), QJsonValue(-1.5), QJsonValue("q"),
                                QJsonValue(QJsonArray{1}), QJsonValue(QJsonObject{{"a", 1}})})
        CHECK(QJsonValue::fromVariant(v.toVariant()) == v);
}

static void testVariantDocument() {
    CHECK(QJsonDocument::fromVariant(QVariant(QVariantMap{{"a", 1}})).isObject());
    CHECK(QJsonDocument::fromVariant(QVariant(QVariantHash{{"a", 1}})).object()["a"].toInt() == 1);
    CHECK(QJsonDocument::fromVariant(QVariant(QVariantList{1, 2})).array().size() == 2);
    CHECK(QJsonDocument::fromVariant(QVariant(QStringList{"p", "q"})).array()[1].toString() == "q");
    CHECK(QJsonDocument::fromVariant(QVariant(3)).isNull());
    CHECK(QJsonDocument::fromVariant(QVariant()).isNull());
    CHECK(QJsonDocument(QJsonObject{{"a", 1}}).toVariant().typeId() == QMetaType::QVariantMap);
    CHECK(QJsonDocument(QJsonArray{1}).toVariant().typeId() == QMetaType::QVariantList);
    CHECK(!QJsonDocument().toVariant().isValid());

    const QJsonDocument doc = QJsonDocument::fromJson("{\"k\": [1, 2, {\"z\": null}]}");
    CHECK(QJsonDocument::fromVariant(doc.toVariant()) == doc);
    CHECK(QJsonValue::fromVariant(QVariant::fromValue(doc)).toObject() == doc.object());
}

static void testVariantMetaTypes() {
    CHECK(QMetaType::fromType<QJsonValue>().id() == QMetaType::QJsonValue);
    CHECK(QMetaType::fromType<QJsonObject>().id() == QMetaType::QJsonObject);
    CHECK(QMetaType::fromType<QJsonArray>().id() == QMetaType::QJsonArray);
    CHECK(QMetaType::fromType<QJsonDocument>().id() == QMetaType::QJsonDocument);
    const QVariant v = QVariant::fromValue(QJsonObject{{"a", 1}});
    CHECK(v.typeId() == QMetaType::QJsonObject && v.value<QJsonObject>()["a"].toInt() == 1);
    CHECK(v == QVariant::fromValue(QJsonObject{{"a", 1}}));
    CHECK(QMetaType::typeName(QMetaType::QJsonArray) == std::string("QJsonArray"));
}

int main() {
    testValueBasics();
    testValueComparison();
    testConstRefAndRef();
    testArray();
    testArraySharing();
    testArrayIterators();
    testObject();
    testObjectIterators();
    testObjectSharing();
    testParseValid();
    testParseStrings();
    testParseErrors();
    testErrorStrings();
    testNesting();
    testLarge();
    testWriter();
    testRoundTrip();
    testDocument();
    testDebug();
    testTypes();
    testVariantScalars();
    testVariantContainers();
    testVariantToVariant();
    testVariantRoundTrip();
    testVariantDocument();
    testVariantMetaTypes();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("all JSON tests passed");
    return 0;
}
