// MIT License - see LICENSE
//
// Tests for the smaller additions to Nt Core: members that complete the view classes, QVariant,
// QUrl, QIODevice and the other value classes.
#include <QByteArray>
#include <QAnyStringView>
#include <QByteArrayView>
#include <QBuffer>
#include <QPartialOrdering>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QList>
#include <QLocale>
#include <QAssociativeIterable>
#include <QMetaAssociation>
#include <QMetaSequence>
#include <QSequentialIterable>
#include <QMetaType>
#include <QPermission>
#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QSet>
#include <QSpan>
#include <QString>
#include <QStringList>
#include <QStringMatcher>
#include <QStringTokenizer>
#include <QStringView>
#include <QSysInfo>
#include <QTimer>
#include <QTemporaryDir>
#include <QUrl>
#include <QUtf8StringView>
#include <QUuid>
#include <QVariant>
#include <QVersionNumber>
#include <QtCore/qyieldcpu.h>

#include <atomic>
#include <optional>
#include <thread>

#include <cstddef>
#include <cstdio>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <set>
#include <map>
#include <list>
#include <string>
#include <variant>
#include <vector>

using namespace Qt::StringLiterals;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testByteArrayView() {
    const QByteArrayView v("  \t42 apples, 7 pears\r\n");
    CHECK(v.trimmed() == QByteArrayView("42 apples, 7 pears"));
    CHECK(QByteArrayView(" \n ").trimmed().isEmpty() && QByteArrayView().trimmed().isEmpty());
    CHECK(v.count('p') == 3 && v.count(QByteArrayView("pears")) == 1 && v.count(QByteArrayView("xyz")) == 0);
    CHECK(QByteArrayView("aaaa").count(QByteArrayView("aa")) == 3); // overlapping
    CHECK(QByteArrayView("abc").count(QByteArrayView()) == 4);

    QByteArrayView w("0123456789");
    w.truncate(8);
    CHECK(w == QByteArrayView("01234567"));
    w.chop(2);
    CHECK(w == QByteArrayView("012345"));
    CHECK(w.slice(2) == QByteArrayView("2345") && w.size() == 4);
    CHECK(w.slice(1, 2) == QByteArrayView("34"));
    CHECK(QByteArrayView::max_size() > 0);

    std::string reversed(QByteArrayView("abc").rbegin(), QByteArrayView("abc").rend());
    // The two views above are distinct objects over the same literal, so iterate one view.
    const QByteArrayView abc("abc");
    reversed.assign(abc.rbegin(), abc.rend());
    CHECK(reversed == "cba");
    CHECK(std::string(abc.crbegin(), abc.crend()) == "cba");

    CHECK(QByteArrayView::fromArray("ab").size() == 3 && QByteArrayView("ab").size() == 2);

    bool ok = false;
    CHECK(QByteArrayView("123").toInt(&ok) == 123 && ok);
    CHECK(QByteArrayView("-7").toShort(&ok) == -7 && ok);
    CHECK(QByteArrayView("70000").toShort(&ok) == 0 && !ok);
    CHECK(QByteArrayView("65535").toUShort(&ok) == 65535 && ok);
    CHECK(QByteArrayView("ff").toUInt(&ok, 16) == 255U && ok);
    CHECK(QByteArrayView("0x10").toLong(&ok, 0) == 16 && ok);
    CHECK(QByteArrayView("18446744073709551615").toULongLong(&ok) == 18446744073709551615ULL && ok);
    CHECK(QByteArrayView("18446744073709551615").toULong(&ok) == 18446744073709551615UL && ok);
    CHECK(QByteArrayView("-9223372036854775808").toLongLong(&ok) == (-9223372036854775807LL - 1) && ok);
    CHECK(QByteArrayView("12x").toInt(&ok) == 0 && !ok);
    CHECK(QByteArrayView("1.5").toDouble(&ok) == 1.5 && ok);
    CHECK(QByteArrayView("2.25").toFloat(&ok) == 2.25F && ok);
    CHECK(QByteArrayView("nope").toDouble(&ok) == 0.0 && !ok);
    CHECK(QByteArray("4294967296").toULong(&ok) == 4294967296UL && ok);
    CHECK(QByteArray("-1").toULong(&ok) == 0 && !ok);

    CHECK(QByteArrayView("Hello").compare(QByteArrayView("hello"), Qt::CaseInsensitive) == 0);
    CHECK(QByteArrayView("Hello").compare(QByteArrayView("hello"), Qt::CaseSensitive) < 0);
    CHECK(QByteArrayView("abc").compare(QByteArrayView("ABD"), Qt::CaseInsensitive) < 0);
    CHECK(QByteArrayView("abcd").compare(QByteArrayView("ABC"), Qt::CaseInsensitive) > 0);

    // UTF-8 validity: overlong forms, surrogates, truncation and values past U+10FFFF are invalid.
    CHECK(QByteArrayView("plain").isValidUtf8() && QByteArrayView().isValidUtf8());
    CHECK(QByteArrayView("gr\xc3\xbc\xc3\x9f e\xe2\x82\xac \xf0\x9f\x98\x80").isValidUtf8());
    CHECK(!QByteArrayView("\xc0\xaf").isValidUtf8());         // overlong '/'
    CHECK(!QByteArrayView("\xe0\x80\xaf").isValidUtf8());     // overlong, three bytes
    CHECK(!QByteArrayView("\xed\xa0\x80").isValidUtf8());     // U+D800
    CHECK(!QByteArrayView("\xf4\x90\x80\x80").isValidUtf8()); // U+110000
    CHECK(!QByteArrayView("\xe2\x82").isValidUtf8());         // truncated
    CHECK(!QByteArrayView("\x80").isValidUtf8() && !QByteArrayView("a\xff").isValidUtf8());
    CHECK(QByteArrayView("\xf4\x8f\xbf\xbf").isValidUtf8()); // U+10FFFF
    CHECK(QByteArray("\xc3\xa9").isValidUtf8() && !QByteArray("\xc3").isValidUtf8());
    CHECK(QUtf8StringView("\xc3\xa9").isValidUtf8() && !QUtf8StringView("\xc3(").isValidUtf8());
}

static void testLatin1() {
    CHECK(QLatin1Char('a') == QLatin1Char('a') && QLatin1Char('a') != QLatin1Char('b'));
    CHECK(QLatin1Char('a') < QLatin1Char('b') && QLatin1Char('b') >= QLatin1Char('a'));
    CHECK(QLatin1Char('\xe9') > QLatin1Char('z')); // by code point, not by signed char
    CHECK('a' == QLatin1Char('a') && QLatin1Char('a') == 'a' && 'a' < QLatin1Char('b') && QLatin1Char('b') > 'a');
    CHECK(QLatin1Char('x') <= 'x' && 'y' != QLatin1Char('x'));
    static_assert(QLatin1Char('q') == 'q');

    CHECK(QLatin1StringView().isNull() && QLatin1StringView().isEmpty() &&
          QLatin1StringView() == QLatin1StringView(""));
    CHECK(!QLatin1StringView("").isNull() && QLatin1StringView("").isEmpty());
    CHECK(QString(QLatin1StringView()).isNull() && !QString(QLatin1StringView("")).isNull());
    const char raw[] = "hello world";
    CHECK(QLatin1StringView(raw, raw + 5) == QLatin1StringView("hello"));
    CHECK(QLatin1StringView("", 0).isEmpty());
    CHECK(QLatin1StringView(QByteArrayView("caf\xe9")).toString() == u"café"_s);
    CHECK(QLatin1StringView(QByteArrayView()).isNull());

    QLatin1StringView s("0123456789");
    CHECK(s.slice(3) == QLatin1StringView("3456789") && s.slice(1, 3) == QLatin1StringView("456"));
    const QLatin1StringView abc("abc");
    CHECK(std::string(abc.rbegin(), abc.rend()) == "cba" && std::string(abc.crbegin(), abc.crend()) == "cba");
    CHECK(QLatin1StringView::max_size() > 0);

    bool ok = false;
    CHECK(QLatin1StringView("-12").toInt(&ok) == -12 && ok);
    CHECK(QLatin1StringView("7f").toUInt(&ok, 16) == 127U && ok);
    CHECK(QLatin1StringView("32768").toShort(&ok) == 0 && !ok);
    CHECK(QLatin1StringView("32768").toUShort(&ok) == 32768 && ok);
    CHECK(QLatin1StringView("123456789012").toLong(&ok) == 123456789012L && ok);
    CHECK(QLatin1StringView("123456789012").toULong(&ok) == 123456789012UL && ok);
    CHECK(QLatin1StringView("-5").toLongLong(&ok) == -5 && ok);
    CHECK(QLatin1StringView("5").toULongLong(&ok) == 5U && ok);
    CHECK(QLatin1StringView("0.5").toDouble(&ok) == 0.5 && ok && QLatin1StringView("0.25").toFloat(&ok) == 0.25F && ok);
    CHECK(QLatin1StringView("x").toDouble(&ok) == 0.0 && !ok);
}

static void testArg() {
    // Several arguments are substituted in one pass: a marker in an argument is not replaced.
    CHECK(u"%1 %2"_s.arg(u"%2"_s, u"x"_s) == u"%2 x"_s);
    CHECK(u"%2-%1-%3"_s.arg(u"a"_s, u"b"_s, u"c"_s) == u"b-a-c"_s);
    CHECK(u"%1%1 %5"_s.arg("one", "five") == u"oneone five"_s);
    CHECK(u"%1 %2 %3"_s.arg(u"a"_s, u"b"_s) == u"a b %3"_s); // a marker left over stays
    CHECK(u"%1"_s.arg(u"a"_s, u"b"_s) == u"a"_s);            // an argument left over is dropped
    CHECK(u"%L1|%2"_s.arg(u"a"_s, QChar(u'b')) == u"a|b"_s);
    CHECK(u"100%% %1 %2"_s.arg(u"a"_s, u"b"_s) == u"100%% a b"_s);
    CHECK(u"%10 %9"_s.arg(u"ten"_s, u"nine"_s) == u"nine ten"_s);

    const QStringView pattern = u"%1 of %2";
    CHECK(pattern.arg(u"3"_s, QLatin1StringView("7")) == u"3 of 7"_s);
    CHECK(pattern.arg(QStringView(u"x")) == u"x of %2"_s);
    CHECK(QStringView(u"<%1>").arg(QChar(u'c')) == u"<c>"_s);
    CHECK(QLatin1StringView("%1, %2!").arg(u"Hello"_s, QStringView(u"World")) == u"Hello, World!"_s);
    CHECK(QLatin1StringView("%2%1").arg(QLatin1StringView("b"), QChar(u'a')) == u"ab"_s);
}

static void testStringViewExtras() {
    QStringList parts;
    for (QStringView token : QStringView(u"a,b,,c").tokenize(u','))
        parts << token.toString();
    CHECK(parts == QStringList({u"a"_s, u"b"_s, u""_s, u"c"_s}));

    parts.clear();
    for (QStringView token : QStringView(u"a,b,,c").tokenize(u',', Qt::SkipEmptyParts))
        parts << token.toString();
    CHECK(parts == QStringList({u"a"_s, u"b"_s, u"c"_s}));

    parts.clear();
    for (QStringView token : QStringView(u"oneXtwoxthree").tokenize(u"x"_s, Qt::CaseInsensitive, Qt::SkipEmptyParts))
        parts << token.toString();
    CHECK(parts == QStringList({u"one"_s, u"two"_s, u"three"_s}));

    const QString text = u"k1=v1;k2=v2"_s;
    parts.clear();
    for (QStringView pair : text.tokenize(u';')) {
        for (QStringView field : pair.tokenize(u'='))
            parts << field.toString();
    }
    CHECK(parts == QStringList({u"k1"_s, u"v1"_s, u"k2"_s, u"v2"_s}));

    // A temporary string is kept alive by its tokenizer.
    parts.clear();
    for (QStringView token : (u"x y "_s + u"z"_s).tokenize(u' '))
        parts << token.toString();
    CHECK(parts == QStringList({u"x"_s, u"y"_s, u"z"_s}));

    QStringView v(u"0123456789");
    CHECK(v.slice(4) == QStringView(u"456789") && v.slice(1, 2) == QStringView(u"56"));
    CHECK(QStringView::fromArray(u"ab").size() == 3 && QStringView::max_size() > 0);
    const QStringView abc(u"abc");
    CHECK(abc.constBegin() == abc.begin() && abc.constEnd() == abc.end());
    wchar_t wide[4] = {};
    CHECK(QStringView(u"a\U0001F600").toWCharArray(wide) == (sizeof(wchar_t) == 4 ? 2 : 3) && wide[0] == L'a');
    CHECK(QStringView(u"abc").localeAwareCompare(QStringView(u"abd")) < 0);
    CHECK(QStringView(u"abc").localeAwareCompare(QStringView(u"abc")) == 0);
}

static void testSpanBytes() {
    int values[] = {1, 2, 3};
    QSpan<int> span(values);
    std::vector<int> reversed(span.crbegin(), span.crend());
    CHECK(reversed == std::vector<int>({3, 2, 1}));

    const QSpan<const std::byte> bytes = as_bytes(span);
    CHECK(bytes.size() == static_cast<qsizetype>(sizeof values));
    CHECK(static_cast<const void *>(bytes.data()) == static_cast<const void *>(values));

    const QSpan<std::byte> writable = as_writable_bytes(span);
    for (std::byte &b : writable)
        b = std::byte{0};
    CHECK(values[0] == 0 && values[1] == 0 && values[2] == 0);

    const int fixed[] = {7};
    CHECK(as_bytes(QSpan<const int>(fixed)).size() == static_cast<qsizetype>(sizeof(int)));
}

static void testVariantTypes() {
    // The classes that have their own id in QMetaType::Type.
    CHECK(QMetaType::fromType<QLocale>().id() == QMetaType::QLocale);
    CHECK(QMetaType::fromType<QRegularExpression>().id() == QMetaType::QRegularExpression);
    CHECK(QMetaType::fromType<QEasingCurve>().id() == QMetaType::QEasingCurve);
    CHECK(QMetaType::fromType<QModelIndex>().id() == QMetaType::QModelIndex);
    CHECK(QMetaType::fromType<QPersistentModelIndex>().id() == QMetaType::QPersistentModelIndex);
    CHECK(QMetaType(QMetaType::QLocale).isValid() && QMetaType::fromName("QLocale").id() == QMetaType::QLocale);
    CHECK(QByteArray(QMetaType(QMetaType::QRegularExpression).name()) == "QRegularExpression");

    const QVariant locale(QLocale(QLocale::German, QLocale::Germany));
    CHECK(locale.typeId() == QMetaType::QLocale && locale.type() == QVariant::Locale);
    CHECK(locale.toLocale() == QLocale(QLocale::German, QLocale::Germany));
    CHECK(QVariant(5).toLocale() == QLocale());

    const QVariant re(QRegularExpression(u"a+b"_s));
    CHECK(re.type() == QVariant::RegularExpression && re.toRegularExpression().pattern() == u"a+b"_s);
    CHECK(re == QVariant(QRegularExpression(u"a+b"_s)) && re != QVariant(QRegularExpression(u"a+"_s)));
    CHECK(QVariant(u"x"_s).toRegularExpression().pattern().isEmpty());

    const QVariant curve(QEasingCurve(QEasingCurve::InOutQuad));
    CHECK(curve.type() == QVariant::EasingCurve && curve.toEasingCurve().type() == QEasingCurve::InOutQuad);
    CHECK(QVariant().toEasingCurve().type() == QEasingCurve::Linear);

    const QUuid id = QUuid::fromString(u"{67c8770b-44f1-410a-ab9a-f9b5446f13ee}");
    const QVariant uuid(id);
    CHECK(uuid.typeId() == QMetaType::QUuid && uuid.toUuid() == id);
    CHECK(QVariant(id.toString()).toUuid() == id && QVariant(u"junk"_s).toUuid().isNull());
    CHECK(uuid.toString() == id.toString());

    const QVariant index{QModelIndex()};
    CHECK(index.type() == QVariant::ModelIndex && !index.toModelIndex().isValid());
    const QVariant persistent{QPersistentModelIndex()};
    CHECK(persistent.type() == QVariant::PersistentModelIndex && !persistent.toPersistentModelIndex().isValid());
    CHECK(index.canConvert<QPersistentModelIndex>() && persistent.canConvert<QModelIndex>());
    CHECK(!index.toPersistentModelIndex().isValid() && !persistent.toModelIndex().isValid());
}

static void testVariantJson() {
    const QJsonObject object{{u"name"_s, u"nt"_s}, {u"major"_s, 6}};
    const QJsonArray array{1, u"two"_s, true};

    const QVariant vo(object);
    const QVariant va(array);
    const QVariant vv(QJsonValue(2.5));
    const QVariant vd{QJsonDocument(object)};
    CHECK(vo.typeId() == QMetaType::QJsonObject && va.typeId() == QMetaType::QJsonArray);
    CHECK(vv.typeId() == QMetaType::QJsonValue && vd.typeId() == QMetaType::QJsonDocument);
    CHECK(vo.toJsonObject() == object && va.toJsonArray() == array && vd.toJsonDocument().object() == object);

    // From the JSON classes to the plain types.
    CHECK(vv.toDouble() == 2.5 && vv.toInt() == 3 && !vv.canConvert<QJsonObject>() == false);
    CHECK(vv.toJsonObject().isEmpty() && vv.toJsonArray().isEmpty());
    CHECK(QVariant(QJsonValue(u"text"_s)).toString() == u"text"_s);
    CHECK(QVariant(QJsonValue(true)).toBool());
    bool ok = true;
    CHECK(QVariant(QJsonValue(u"12"_s)).toInt(&ok) == 0 && !ok); // a JSON string is not a number
    CHECK(QVariant(QJsonValue(Q_INT64_C(9007199254740991))).toLongLong() == Q_INT64_C(9007199254740991));
    CHECK(vo.toMap().value(u"major"_s).toInt() == 6 && vo.toHash().value(u"name"_s).toString() == u"nt"_s);
    CHECK(va.toList().size() == 3 && va.toList().at(1).toString() == u"two"_s);
    CHECK(va.toStringList().isEmpty()); // not every element is a string
    CHECK(QVariant(QJsonArray{u"a"_s, u"b"_s}).toStringList() == QStringList({u"a"_s, u"b"_s}));
    CHECK(vd.toMap().size() == 2 && vd.toJsonObject() == object && vd.toJsonValue().isObject());
    CHECK(!vo.canConvert<QVariantList>() && !va.canConvert<QVariantMap>() && !vo.canConvert<int>());

    // From the plain types to the JSON classes.
    CHECK(QVariant(7).toJsonValue() == QJsonValue(7));
    CHECK(QVariant(true).toJsonValue() == QJsonValue(true));
    CHECK(QVariant(1.5f).toJsonValue() == QJsonValue(1.5));
    CHECK(QVariant(u"s"_s).toJsonValue() == QJsonValue(u"s"_s));
    CHECK(QVariant::fromValue(nullptr).toJsonValue().isNull());
    CHECK(QVariant().toJsonValue().isNull() && QVariant(QDate(2024, 1, 1)).toJsonValue().isNull());
    CHECK(QVariant(static_cast<short>(-3)).toJsonValue() == QJsonValue(-3));
    const QVariantMap map{{u"k"_s, 1}};
    CHECK(QVariant(map).toJsonObject() == QJsonObject({{u"k"_s, 1}}));
    CHECK(QVariant(map).toJsonValue().isObject() && QVariant(map).toJsonDocument().isObject());
    CHECK(QVariant(QVariantHash{{u"k"_s, 1}}).toJsonObject().value(u"k"_s).toInt() == 1);
    CHECK(QVariant(QVariantList{1, 2}).toJsonArray() == QJsonArray({1, 2}));
    CHECK(QVariant(QStringList{u"a"_s}).toJsonArray() == QJsonArray({u"a"_s}));
    CHECK(QVariant(QVariantList{1}).toJsonDocument().isArray());
    CHECK(QVariant(map).toJsonArray().isEmpty() && QVariant(3).toJsonObject().isEmpty());
    CHECK(vo.toJsonValue().isObject() && va.toJsonValue().isArray() && va.toJsonDocument().isArray());

    QVariant converted(array);
    CHECK(converted.convert(QMetaType(QMetaType::QVariantList)) && converted.toList().size() == 3);
    converted = QVariant(QJsonValue(4));
    CHECK(converted == QVariant(4)); // equal after conversion
    CHECK(!converted.convert(QMetaType(QMetaType::QJsonArray)));
}

static void testVariantCompare() {
    CHECK(QVariant::compare(QVariant(1), QVariant(2)) == QPartialOrdering::Less);
    CHECK(QVariant::compare(QVariant(2), QVariant(2)) == QPartialOrdering::Equivalent);
    CHECK(QVariant::compare(QVariant(3), QVariant(2)) == QPartialOrdering::Greater);
    // Numbers of different types compare by value.
    CHECK(QVariant::compare(QVariant(1), QVariant(2.5)) == QPartialOrdering::Less);
    CHECK(QVariant::compare(QVariant(-1), QVariant(1u)) == QPartialOrdering::Less);
    CHECK(QVariant::compare(QVariant(1u), QVariant(-1)) == QPartialOrdering::Greater);
    CHECK(QVariant::compare(QVariant(Q_UINT64_C(18446744073709551615)), QVariant(Q_INT64_C(5))) ==
          QPartialOrdering::Greater);
    CHECK(QVariant::compare(QVariant(2.0f), QVariant(2)) == QPartialOrdering::Equivalent);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(QVariant::compare(QVariant(nan), QVariant(1.0)) == QPartialOrdering::Unordered);
    CHECK(QVariant::compare(QVariant(nan), QVariant(nan)) == QPartialOrdering::Unordered);

    CHECK(QVariant::compare(QVariant(u"b"_s), QVariant(u"a"_s)) == QPartialOrdering::Greater);
    CHECK(QVariant::compare(QVariant(QDate(2020, 1, 1)), QVariant(QDate(2021, 1, 1))) == QPartialOrdering::Less);
    // No conversion between types, and no order for a type without operator<.
    CHECK(QVariant::compare(QVariant(1), QVariant(u"1"_s)) == QPartialOrdering::Unordered);
    CHECK(QVariant::compare(QVariant(QVariantMap()), QVariant(QVariantMap())) == QPartialOrdering::Unordered);
    CHECK(QVariant::compare(QVariant(), QVariant()) == QPartialOrdering::Unordered);
    CHECK(QVariant::compare(QVariant(1), QVariant()) == QPartialOrdering::Unordered);

    const QMetaType intType = QMetaType::fromType<int>();
    const int one = 1;
    const int two = 2;
    CHECK(intType.compare(&one, &two) == QPartialOrdering::Less);
    CHECK(intType.compare(&two, &one) == QPartialOrdering::Greater);
    CHECK(intType.compare(&one, &one) == QPartialOrdering::Equivalent);
    CHECK(intType.compare(&one, nullptr) == QPartialOrdering::Unordered);
    CHECK(QMetaType().compare(&one, &two) == QPartialOrdering::Unordered);
}

static void testVariantInPlace() {
    QVariant v(std::in_place_type<QString>, 3, QChar(u'x'));
    CHECK(v.typeId() == QMetaType::QString && v.toString() == u"xxx"_s && !v.isNull());
    const QVariant list(std::in_place_type<QList<int>>, {1, 2, 3});
    CHECK(list.value<QList<int>>() == QList<int>({1, 2, 3}));
    const QVariant number(std::in_place_type<double>, 1.5);
    CHECK(number.typeId() == QMetaType::Double && number.toDouble() == 1.5);

    QString &text = v.emplace<QString>(u"abc"_s);
    text.append(u'd');
    CHECK(v.toString() == u"abcd"_s);
    std::vector<int> &numbers = v.emplace<std::vector<int>>({4, 5});
    numbers.push_back(6);
    CHECK(v.value<std::vector<int>>() == std::vector<int>({4, 5, 6}));
    CHECK(v.emplace<int>() == 0 && v.typeId() == QMetaType::Int);

    // get_if() reaches the stored value without converting it.
    QVariant s(u"shared"_s);
    const QVariant copy = s;
    CHECK(get_if<int>(&s) == nullptr && get_if<QString>(static_cast<QVariant *>(nullptr)) == nullptr);
    QString *stored = get_if<QString>(&s);
    CHECK(stored != nullptr);
    if (stored)
        stored->append(u'!');
    CHECK(s.toString() == u"shared!"_s && copy.toString() == u"shared"_s); // detached from the copy
    const QString *constStored = get_if<QString>(&copy);
    CHECK(constStored && *constStored == u"shared"_s);
    QVariant invalid;
    CHECK(get_if<QString>(&invalid) == nullptr);

    get<QString>(s) = u"set"_s;
    CHECK(get<QString>(copy) == u"shared"_s && get<QString>(std::as_const(s)) == u"set"_s);
    const QString moved = get<QString>(std::move(s));
    CHECK(moved == u"set"_s);

    using Alternative = std::variant<std::monostate, int, QString>;
    CHECK(!QVariant::fromStdVariant(Alternative()).isValid());
    CHECK(QVariant::fromStdVariant(Alternative(7)).typeId() == QMetaType::Int);
    const Alternative named(u"name"_s);
    CHECK(QVariant::fromStdVariant(named).toString() == u"name"_s);
    CHECK(QVariant::fromStdVariant(Alternative(u"moved"_s)).toString() == u"moved"_s);
}

static void testUrl() {
    // Punycode, with the sample strings of RFC 3492 and common IDN examples.
    CHECK(QUrl::toAce(u"bücher.example"_s) == "xn--bcher-kva.example");
    CHECK(QUrl::toAce(u"MÜNCHEN.de"_s) == "xn--mnchen-3ya.de");
    CHECK(QUrl::toAce(u"例え.テスト"_s) == "xn--r8jz45g.xn--zckzah");
    CHECK(QUrl::toAce(u"日本語.jp"_s) == "xn--wgv71a119e.jp");
    CHECK(QUrl::toAce(u"他们为什么不说中文"_s) == "xn--ihqwcrb4cv8a8dqg056pqjye");
    CHECK(QUrl::toAce(u"Pročprostěnemluvíčesky"_s) == "xn--proprostnemluvesky-uyb24dma41a");
    CHECK(QUrl::toAce(QString::fromUtf8("\xf0\x9f\x92\xa9.la")) == "xn--ls8h.la"); // outside the BMP
    CHECK(QUrl::toAce(u"Example.COM"_s) == "example.com");
    CHECK(QUrl::toAce(u"example.com."_s) == "example.com.");           // the root label
    CHECK(QUrl::toAce(u"例え。テスト"_s) == "xn--r8jz45g.xn--zckzah"); // ideographic full stop
    CHECK(QUrl::toAce(u"faß.de"_s) == "xn--fa-hia.de");
    CHECK(QUrl::toAce(u"faß.de"_s, QUrl::AceTransitionalProcessing) == "fass.de");
    CHECK(QUrl::toAce(u"a..b"_s).isEmpty() && QUrl::toAce(QString()).isEmpty());
    CHECK(QUrl::toAce(QString(64, QChar(u'a')) + u".com"_s).isEmpty());
    CHECK(QUrl::toAce(QString(63, QChar(u'a')) + u".com"_s).size() == 67);
    CHECK(QUrl::toAce(QString(60, QChar(u'ü'))).isEmpty()); // too long once encoded

    CHECK(QUrl::fromAce("xn--bcher-kva.example") == u"bücher.example"_s);
    CHECK(QUrl::fromAce("XN--MNCHEN-3YA.DE") == u"münchen.de"_s);
    CHECK(QUrl::fromAce("xn--r8jz45g.xn--zckzah") == u"例え.テスト"_s);
    CHECK(QUrl::fromAce("xn--ls8h.la") == QString::fromUtf8("\xf0\x9f\x92\xa9.la"));
    CHECK(QUrl::fromAce("xn--ihqwcrb4cv8a8dqg056pqjye") == u"他们为什么不说中文"_s);
    CHECK(QUrl::fromAce("www.example.com") == u"www.example.com"_s);
    CHECK(QUrl::fromAce("xn--.com") == u"xn--.com"_s);                       // nothing to decode
    CHECK(QUrl::fromAce("xn--a!b.com") == u"xn--a!b.com"_s);                 // not Punycode: kept
    CHECK(QUrl::fromAce("xn--99999999999.com") == u"xn--99999999999.com"_s); // overflows
    CHECK(QUrl::fromAce("xn--abc-.com") == u"xn--abc-.com"_s);               // would be plain ASCII
    for (const QString &name : {u"bücher.example"_s, u"ελληνικά.gr"_s, u"한국어.kr"_s, u"a-ü-b.x"_s})
        CHECK(QUrl::fromAce(QUrl::toAce(name)) == name);

    // An internationalized host is kept as given and encoded for the wire.
    const QUrl idn(u"http://bücher.example/käse?q=1"_s);
    CHECK(idn.isValid() && idn.host() == u"bücher.example"_s);
    CHECK(idn.host(QUrl::FullyEncoded) == u"xn--bcher-kva.example"_s);
    CHECK(idn.toEncoded() == "http://xn--bcher-kva.example/k%C3%A4se?q=1");
    CHECK(idn.authority(QUrl::FullyEncoded) == u"xn--bcher-kva.example"_s);
    CHECK(idn.toString().startsWith(u"http://bücher.example/"_s));
    CHECK(QUrl(u"http://xn--bcher-kva.example/"_s).host() == u"xn--bcher-kva.example"_s);
    CHECK(QUrl(u"http://xn--bcher-kva.example/"_s) == QUrl(u"http://bücher.example/"_s));

    // fromUserInput()
    CHECK(QUrl::fromUserInput(u"example.com"_s) == QUrl(u"http://example.com"_s));
    CHECK(QUrl::fromUserInput(u"  example.com/a/b.html?x=1  "_s) == QUrl(u"http://example.com/a/b.html?x=1"_s));
    CHECK(QUrl::fromUserInput(u"localhost:8080"_s) == QUrl(u"http://localhost:8080"_s));
    CHECK(QUrl::fromUserInput(u"cafe:80/x"_s) == QUrl(u"http://cafe:80/x"_s));
    CHECK(QUrl::fromUserInput(u"ftp.example.org/pub"_s) == QUrl(u"ftp://ftp.example.org/pub"_s));
    CHECK(QUrl::fromUserInput(u"https://nt.example/a?b#c"_s) == QUrl(u"https://nt.example/a?b#c"_s));
    CHECK(QUrl::fromUserInput(u"mailto:someone@example.com"_s) == QUrl(u"mailto:someone@example.com"_s));
    CHECK(QUrl::fromUserInput(u"/etc/hosts"_s) == QUrl(u"file:///etc/hosts"_s));
    CHECK(QUrl::fromUserInput(u"::1"_s) == QUrl(u"http://[::1]"_s));
    CHECK(QUrl::fromUserInput(u"FE80::1"_s).host() == u"fe80::1"_s);
    CHECK(QUrl::fromUserInput(u"192.168.0.1"_s) == QUrl(u"http://192.168.0.1"_s));
    CHECK(!QUrl::fromUserInput(u"   "_s).isValid() && !QUrl::fromUserInput(QString()).isValid());
    {
        QTemporaryDir dir;
        CHECK(dir.isValid());
        QFile file(dir.filePath(u"notes.txt"_s));
        CHECK(file.open(QIODevice::WriteOnly));
        file.close();
        const QUrl existing = QUrl::fromUserInput(u"notes.txt"_s, dir.path());
        CHECK(existing.isLocalFile() && existing.toLocalFile() == dir.filePath(u"notes.txt"_s));
        // A name that is not a file there is a host, unless files are assumed.
        CHECK(QUrl::fromUserInput(u"example.com"_s, dir.path()) == QUrl(u"http://example.com"_s));
        const QUrl assumed = QUrl::fromUserInput(u"new.txt"_s, dir.path(), QUrl::AssumeLocalFile);
        CHECK(assumed.isLocalFile() && assumed.toLocalFile() == dir.filePath(u"new.txt"_s));
        CHECK(QUrl::fromUserInput(u"http://example.com"_s, dir.path(), QUrl::AssumeLocalFile) ==
              QUrl(u"http://example.com"_s));
    }

    // PreferLocalFile and RemoveFilename
    const QUrl local = QUrl::fromLocalFile(u"/tmp/a dir/report.txt"_s);
    CHECK(local.toString(QUrl::PreferLocalFile) == u"/tmp/a dir/report.txt"_s);
    CHECK(local.toString() == u"file:///tmp/a dir/report.txt"_s);
    CHECK(QUrl(u"file:///tmp/x?y"_s).toString(QUrl::PreferLocalFile) == u"file:///tmp/x?y"_s);
    CHECK(QUrl(u"http://h/a/b"_s).toString(QUrl::PreferLocalFile) == u"http://h/a/b"_s);
    CHECK(local.toString(QUrl::PreferLocalFile | QUrl::RemoveFilename) == u"/tmp/a dir/"_s);
    const QUrl page(u"http://example.com/docs/guide/index.html?v=2#top"_s);
    CHECK(page.adjusted(QUrl::RemoveFilename) == QUrl(u"http://example.com/docs/guide/?v=2#top"_s));
    CHECK(page.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path() == u"/docs/guide"_s);
    CHECK(QUrl(u"http://example.com/docs/"_s).adjusted(QUrl::RemoveFilename).path() == u"/docs/"_s);
    CHECK(QUrl(u"file.txt"_s).adjusted(QUrl::RemoveFilename).path().isEmpty());
    CHECK(page.toString(QUrl::RemoveFilename | QUrl::RemoveQuery | QUrl::RemoveFragment) ==
          u"http://example.com/docs/guide/"_s);

    // matches(), swap() and assignment from a string
    CHECK(QUrl(u"http://example.com/docs/#intro"_s)
              .matches(QUrl(u"http://example.com/docs"_s), QUrl::RemoveFragment | QUrl::StripTrailingSlash));
    CHECK(!QUrl(u"http://example.com/docs/#intro"_s).matches(QUrl(u"http://example.com/docs"_s), QUrl::RemoveFragment));
    CHECK(QUrl(u"http://u:p@h:81/x"_s).matches(QUrl(u"https://other/x"_s), QUrl::RemoveScheme | QUrl::RemoveAuthority));
    CHECK(QUrl(u"http://u:p@h:81/x"_s).matches(QUrl(u"http://h/x"_s), QUrl::RemoveUserInfo | QUrl::RemovePort));
    CHECK(page.matches(page, QUrl::None) && !page.matches(local, QUrl::None));
    QUrl first(u"http://a.example/"_s);
    QUrl second(u"http://b.example/"_s);
    first.swap(second);
    CHECK(first.host() == u"b.example"_s && second.host() == u"a.example"_s);
    QUrl assigned;
    assigned = u"http://c.example/x"_s;
    CHECK(assigned.host() == u"c.example"_s && assigned.path() == u"/x"_s);

    // Lists
    const QStringList strings{u"http://a.example/p#f"_s, u"file:///tmp/x"_s, u"mailto:me@example.com"_s};
    const QList<QUrl> urls = QUrl::fromStringList(strings);
    CHECK(urls.size() == 3 && urls.at(0).fragment() == u"f"_s && urls.at(1).isLocalFile());
    CHECK(QUrl::toStringList(urls) == strings);
    CHECK(QUrl::toStringList(urls, QUrl::RemoveFragment).at(0) == u"http://a.example/p"_s);
    CHECK(QUrl::toStringList(urls, QUrl::PreferLocalFile).at(1) == u"/tmp/x"_s);
    CHECK(QUrl::fromStringList({}).isEmpty() && QUrl::toStringList({}).isEmpty());
    CHECK(!QUrl::fromStringList({u"http://a b/"_s}, QUrl::StrictMode).at(0).isValid());
}

static void testIoChannels() {
    QBuffer buffer;
    CHECK(buffer.readChannelCount() == 0 && buffer.writeChannelCount() == 0);
    CHECK(buffer.open(QIODevice::ReadOnly));
    CHECK(buffer.readChannelCount() == 1 && buffer.writeChannelCount() == 0);
    buffer.close();
    CHECK(buffer.open(QIODevice::ReadWrite));
    CHECK(buffer.readChannelCount() == 1 && buffer.writeChannelCount() == 1);
    CHECK(buffer.currentReadChannel() == 0 && buffer.currentWriteChannel() == 0);
    buffer.setCurrentWriteChannel(-3); // ignored
    CHECK(buffer.currentWriteChannel() == 0);
    buffer.setCurrentWriteChannel(2);
    buffer.setCurrentReadChannel(2);
    CHECK(buffer.currentWriteChannel() == 2 && buffer.currentReadChannel() == 2);
    buffer.setCurrentReadChannel(0);
    buffer.setCurrentWriteChannel(0);
    CHECK(buffer.write("abc") == 3 && buffer.data() == "abc");

    // The signals carry the channel.
    int readChannel = -1;
    int writeChannel = -1;
    qint64 written = 0;
    QObject::connect(&buffer, &QIODevice::channelReadyRead, [&](int channel) { readChannel = channel; });
    QObject::connect(&buffer, &QIODevice::channelBytesWritten, [&](int channel, qint64 bytes) {
        writeChannel = channel;
        written = bytes;
    });
    buffer.channelReadyRead(1);
    buffer.channelBytesWritten(0, 42);
    CHECK(readChannel == 1 && writeChannel == 0 && written == 42);
}

static void testVersionAndUuid() {
    // fromString() takes any kind of string.
    qsizetype suffix = -1;
    CHECK(QVersionNumber::fromString("6.5.1-beta", &suffix) == QVersionNumber(6, 5, 1) && suffix == 5);
    CHECK(QVersionNumber::fromString(u"6.5.1"_s) == QVersionNumber(6, 5, 1));
    CHECK(QVersionNumber::fromString(QStringView(u"1.2rc"), &suffix) == QVersionNumber(1, 2) && suffix == 3);
    CHECK(QVersionNumber::fromString(QLatin1StringView("10.20.30.40")).segmentCount() == 4);
    CHECK(QVersionNumber::fromString(QByteArray("3.14")) == QVersionNumber(3, 14));
    CHECK(QVersionNumber::fromString(QUtf8StringView("2.0\xc3\xa4"), &suffix) == QVersionNumber(2, 0) && suffix == 3);
    CHECK(QVersionNumber::fromString(QAnyStringView()).isNull());
    CHECK(QVersionNumber::fromString("x1", &suffix).isNull() && suffix == 0);

    const QVersionNumber v{6, 8, 0, 2};
    CHECK(std::vector<int>(v.begin(), v.end()) == (std::vector<int>{6, 8, 0, 2}));
    CHECK(std::vector<int>(v.rbegin(), v.rend()) == (std::vector<int>{2, 0, 8, 6}));
    CHECK(v.cend() - v.cbegin() == 4 && v.constEnd() - v.constBegin() == 4 && *v.crbegin() == 2);
    CHECK(v.crend() - v.crbegin() == 4);
    int sum = 0;
    for (int segment : v)
        sum += segment;
    CHECK(sum == 16 && QVersionNumber().begin() == QVersionNumber().end());
    CHECK(qHash(v) == qHash(QVersionNumber{6, 8, 0, 2}) && qHash(v) != qHash(QVersionNumber(6, 8)));
    CHECK(qHash(v, 1) != qHash(v, 2));
    QSet<QVersionNumber> versions{QVersionNumber(1, 0), QVersionNumber(1, 1), QVersionNumber(1, 0)};
    CHECK(versions.size() == 2 && versions.contains(QVersionNumber(1, 1)));

    const QUuid id(0x67c8770b, 0x44f1, 0x410a, 0xab, 0x9a, 0xf9, 0xb5, 0x44, 0x6f, 0x13, 0xee);
    CHECK(QUuid::fromString("{67c8770b-44f1-410a-ab9a-f9b5446f13ee}") == id);
    CHECK(QUuid::fromString(u"67C8770B-44F1-410A-AB9A-F9B5446F13EE"_s) == id);
    CHECK(QUuid::fromString(QLatin1StringView("67c8770b44f1410aab9af9b5446f13ee")) == id);
    CHECK(QUuid::fromString(QByteArray("{67c8770b-44f1-410a-ab9a-f9b5446f13ee}")) == id);
    CHECK(QUuid::fromString(QUtf8StringView("67c8770b-44f1-410a-ab9a-f9b5446f13ee")) == id);
    CHECK(QUuid(QLatin1StringView("{67c8770b-44f1-410a-ab9a-f9b5446f13ee}")) == id);
    CHECK(QUuid(QAnyStringView(u"67c8770b-44f1-410a-ab9a-f9b5446f13ee")) == id);
    CHECK(QUuid::fromString(QAnyStringView()).isNull() && QUuid::fromString("").isNull());
    CHECK(QUuid::fromString(static_cast<const char *>(nullptr)).isNull());
    // 36 bytes of UTF-8, but not 36 characters.
    CHECK(QUuid::fromString("67c8770b-44f1-410a-ab9a-f9b5446f13\xc3\xa4").isNull());
    CHECK(QUuid::fromString(QString(100000, QChar(u'a'))).isNull());

    const QUuid::Id128Bytes big = id.toBytes();
    CHECK(QByteArray(reinterpret_cast<const char *>(big.data), 16) == id.toRfc4122());
    CHECK(big.data[0] == 0x67 && big.data[15] == 0xee);
    const QUuid::Id128Bytes little = id.toBytes(QSysInfo::LittleEndian);
    CHECK(little.data[0] == 0xee && little.data[15] == 0x67);
    CHECK(QUuid::fromBytes(big.data) == id && QUuid::fromBytes(little.data, QSysInfo::LittleEndian) == id);
    CHECK(QUuid::fromBytes(little.data) != id && QUuid::fromBytes(nullptr).isNull());
    CHECK(QUuid(big) == id && QUuid(little, QSysInfo::LittleEndian) == id);
    CHECK(QUuid().toBytes().data64[0] == 0 && QUuid().toBytes().data64[1] == 0);
    const QUuid random = QUuid::createUuid();
    CHECK(QUuid(random.toBytes(QSysInfo::ByteOrder), QSysInfo::ByteOrder) == random);
}

static void testStringListAndUninitialized() {
    QStringList list{"alpha", "Beta", "gamma", "beta", "alphabet"};
    const QString beta = QStringLiteral("beta");
    const QStringView view(beta);

    // The view overloads.
    CHECK(list.contains(view) && !list.contains(QStringView(u"delta")));
    CHECK(list.contains(QLatin1StringView("GAMMA"), Qt::CaseInsensitive) && !list.contains(QLatin1StringView("GAMMA")));
    CHECK(list.indexOf(view) == 3 && list.indexOf(view, 0, Qt::CaseInsensitive) == 1);
    CHECK(list.indexOf(QLatin1StringView("beta"), 2, Qt::CaseInsensitive) == 3);
    CHECK(list.lastIndexOf(view) == 3 && list.lastIndexOf(view, 2) == -1);
    CHECK(list.lastIndexOf(view, 2, Qt::CaseInsensitive) == 1);
    CHECK(list.lastIndexOf(QLatin1StringView("BETA"), -1, Qt::CaseInsensitive) == 3);
    CHECK(list.join(QStringView(u"/")) == "alpha/Beta/gamma/beta/alphabet");
    CHECK(QStringList({"a", "b"}).join(QLatin1StringView(", ")) == "a, b");
    CHECK(list.filter(QStringView(u"alpha")) == QStringList({"alpha", "alphabet"}));
    CHECK(list.filter(QLatin1StringView("BET"), Qt::CaseInsensitive) == QStringList({"Beta", "beta", "alphabet"}));

    // lastIndexOf() with a case sensitivity, and its from argument.
    CHECK(list.lastIndexOf(beta) == 3 && list.lastIndexOf(beta, -1, Qt::CaseInsensitive) == 3);
    CHECK(list.lastIndexOf(beta, -3, Qt::CaseInsensitive) == 1 && list.lastIndexOf(beta, -5) == -1);
    CHECK(list.lastIndexOf(beta, 100) == 3 && QStringList().lastIndexOf(beta) == -1);
    CHECK(list.lastIndexOf(QStringLiteral("Beta")) == 1 && list.lastIndexOf(QStringLiteral("delta")) == -1);

    // String literals still select the QString overloads.
    CHECK(list.contains("gamma") && list.contains(u"gamma") && !list.contains("Gamma"));
    CHECK(list.indexOf("gamma") == 2 && list.lastIndexOf("alpha") == 0 && list.lastIndexOf(u"alpha") == 0);
    CHECK(list.join(", ").startsWith("alpha, Beta") && list.join(u"-").startsWith(u"alpha-Beta"));
    CHECK(list.filter("gam").size() == 1);

    // filter() with a matcher.
    CHECK(list.filter(QStringMatcher(QStringLiteral("bet"))) == QStringList({"beta", "alphabet"}));
    CHECK(list.filter(QStringMatcher(QStringLiteral("BET"), Qt::CaseInsensitive)).size() == 3);
    CHECK(list.filter(QStringMatcher(QStringLiteral("zzz"))).isEmpty());

    // replaceInStrings() with views.
    QStringList r{"one two", "two three"};
    const QString two = QStringLiteral("two"), deux = QStringLiteral("2");
    r.replaceInStrings(QStringView(two), QStringView(deux));
    CHECK(r == QStringList({"one 2", "2 three"}));
    r.replaceInStrings(QStringView(deux), QStringLiteral("TWO"));
    CHECK(r == QStringList({"one TWO", "TWO three"}));
    r.replaceInStrings(QStringLiteral("two"), QStringView(deux), Qt::CaseInsensitive);
    CHECK(r == QStringList({"one 2", "2 three"}));
    r.replaceInStrings("2", "two"); // literals: the QString overload
    CHECK(r == QStringList({"one two", "two three"}));

    // Qt::Uninitialized.
    QByteArray bytes(4, Qt::Uninitialized);
    CHECK(bytes.size() == 4 && !bytes.isNull());
    bytes.resizeForOverwrite(6);
    CHECK(bytes.size() == 6);
    bytes.fill('x');
    CHECK(bytes == "xxxxxx");
    bytes.resizeForOverwrite(2);
    CHECK(bytes == "xx");
    CHECK(QByteArray(0, Qt::Uninitialized).isEmpty() && QByteArray(-3, Qt::Uninitialized).isEmpty());

    QString str(3, Qt::Uninitialized);
    CHECK(str.size() == 3 && !str.isNull());
    str.resizeForOverwrite(5);
    CHECK(str.size() == 5);
    str.fill(QChar(u'y'));
    CHECK(str == "yyyyy");
    str.resizeForOverwrite(1);
    CHECK(str == "y");

    QList<int> ints(3, Qt::Uninitialized);
    CHECK(ints.size() == 3);
    ints.resizeForOverwrite(5);
    CHECK(ints.size() == 5);
    for (qsizetype i = 0; i < ints.size(); ++i)
        ints[i] = static_cast<int>(i);
    ints.resizeForOverwrite(2);
    CHECK(ints == QList<int>({0, 1}));
    QList<QString> strings(2, Qt::Uninitialized);
    CHECK(strings.size() == 2 && strings.at(0).isEmpty());
}

namespace {

class PermissionReceiver : public QObject {
public:
    void answer(const QPermission &permission) { answers.push_back(permission.status()); }
    std::vector<Qt::PermissionStatus> answers;
};

} // namespace

static void testPermissions() {
    // The permission classes keep their settings and can be copied, moved and swapped.
    QLocationPermission location;
    CHECK(location.accuracy() == QLocationPermission::Approximate);
    CHECK(location.availability() == QLocationPermission::WhenInUse);
    location.setAccuracy(QLocationPermission::Precise);
    location.setAvailability(QLocationPermission::Always);
    QLocationPermission copy = location;
    CHECK(copy.accuracy() == QLocationPermission::Precise && copy.availability() == QLocationPermission::Always);
    QLocationPermission other;
    other.swap(copy);
    CHECK(other.accuracy() == QLocationPermission::Precise && copy.accuracy() == QLocationPermission::Approximate);

    QCalendarPermission calendar;
    CHECK(calendar.accessMode() == QCalendarPermission::ReadOnly);
    calendar.setAccessMode(QCalendarPermission::ReadWrite);
    CHECK(calendar.accessMode() == QCalendarPermission::ReadWrite);
    QContactsPermission contacts;
    contacts.setAccessMode(QContactsPermission::ReadWrite);
    CHECK(contacts.accessMode() == QContactsPermission::ReadWrite);

    QBluetoothPermission bluetooth;
    CHECK(bluetooth.communicationModes() == QBluetoothPermission::Default);
    bluetooth.setCommunicationModes(QBluetoothPermission::Access);
    CHECK(bluetooth.communicationModes() == QBluetoothPermission::Access);
    bluetooth.setCommunicationModes({});
    CHECK(bluetooth.communicationModes() == QBluetoothPermission::Default);

    // QPermission holds a copy of one of them, with its type.
    const QPermission invalid;
    CHECK(!invalid.type().isValid() && invalid.status() == Qt::PermissionStatus::Undetermined);
    CHECK(!invalid.value<QCameraPermission>().has_value());
    const QPermission held = location;
    CHECK(held.type() == QMetaType::fromType<QLocationPermission>());
    CHECK(held.status() == Qt::PermissionStatus::Undetermined);
    const std::optional<QLocationPermission> back = held.value<QLocationPermission>();
    CHECK(back.has_value() && back->accuracy() == QLocationPermission::Precise);
    CHECK(!held.value<QCameraPermission>().has_value());
    location.setAccuracy(QLocationPermission::Approximate);
    CHECK(held.value<QLocationPermission>()->accuracy() == QLocationPermission::Precise);

    // There are no permission prompts: every valid permission is granted.
    CHECK(qApp->checkPermission(QCameraPermission()) == Qt::PermissionStatus::Granted);
    CHECK(qApp->checkPermission(QMicrophonePermission()) == Qt::PermissionStatus::Granted);
    CHECK(qApp->checkPermission(held) == Qt::PermissionStatus::Granted);
    CHECK(qApp->checkPermission(invalid) == Qt::PermissionStatus::Denied);

    // requestPermission() answers from the event loop, with a functor, a member function or no context.
    PermissionReceiver receiver;
    std::vector<Qt::PermissionStatus> answers;
    QMetaType answeredType;
    qApp->requestPermission(QMicrophonePermission(), &receiver, [&](const QPermission &p) {
        answers.push_back(p.status());
        answeredType = p.type();
    });
    qApp->requestPermission(QCameraPermission(), &receiver, &PermissionReceiver::answer);
    qApp->requestPermission(invalid, [&](const QPermission &p) { answers.push_back(p.status()); });
    CHECK(answers.empty() && receiver.answers.empty());
    QCoreApplication::processEvents();
    CHECK((answers == std::vector<Qt::PermissionStatus>{Qt::PermissionStatus::Granted, Qt::PermissionStatus::Denied}));
    CHECK(answeredType == QMetaType::fromType<QMicrophonePermission>());
    CHECK((receiver.answers == std::vector<Qt::PermissionStatus>{Qt::PermissionStatus::Granted}));

    // The callback is not called once its context is gone.
    bool called = false;
    {
        QObject context;
        qApp->requestPermission(QCameraPermission(), &context, [&](const QPermission &) { called = true; });
    }
    QCoreApplication::processEvents();
    CHECK(!called);
}

static void testYieldCpu() {
    std::atomic<bool> ready{false};
    std::thread setter([&] { ready.store(true, std::memory_order_release); });
    while (!ready.load(std::memory_order_acquire))
        qYieldCpu();
    setter.join();
    CHECK(ready.load());
}

namespace {

// Walks the container with const iterators and collects its values.
template <typename V> QList<V> valuesOf(const QMetaSequence &meta, const void *container) {
    QList<V> out;
    void *it = meta.constBegin(container);
    void *end = meta.constEnd(container);
    V value{};
    for (; !meta.compareConstIterator(it, end); meta.advanceConstIterator(it, 1)) {
        meta.valueAtConstIterator(it, &value);
        out.append(value);
    }
    meta.destroyConstIterator(it);
    meta.destroyConstIterator(end);
    return out;
}

// Can be copied, but not assigned or default-constructed.
struct Fixed {
    explicit Fixed(int v) : value(v) {}
    const int value;
    bool operator==(const Fixed &o) const { return value == o.value; }
    bool operator<(const Fixed &o) const { return value < o.value; }
};

} // namespace

static void testMetaSequence() {
    // A QList can do everything.
    const QMetaSequence list = QMetaSequence::fromContainer<QList<int>>();
    CHECK(list == QMetaSequence::fromContainer<QList<int>>() && list != QMetaSequence::fromContainer<QList<double>>());
    CHECK(list.valueMetaType() == QMetaType::fromType<int>());
    CHECK(list.hasInputIterator() && list.hasForwardIterator() && list.hasBidirectionalIterator() &&
          list.hasRandomAccessIterator());
    CHECK(list.hasSize() && list.canClear() && list.hasIterator() && list.hasConstIterator() && list.isSortable());
    CHECK(list.canAddValueAtBegin() && list.canAddValueAtEnd() && list.canRemoveValueAtBegin() &&
          list.canRemoveValueAtEnd());
    CHECK(list.canGetValueAtIndex() && list.canSetValueAtIndex() && list.canInsertValueAtIterator() &&
          list.canEraseValueAtIterator() && list.canEraseRangeAtIterator());
    QList<int> values = {1, 2, 3};
    CHECK(list.size(&values) == 3);
    int v = 0;
    list.valueAtIndex(&values, 1, &v);
    CHECK(v == 2);
    v = 20;
    list.setValueAtIndex(&values, 1, &v);
    v = 0;
    list.addValueAtBegin(&values, &v);
    v = 4;
    list.addValueAtEnd(&values, &v);
    v = 5;
    list.addValue(&values, &v);
    CHECK((values == QList<int>{0, 1, 20, 3, 4, 5}));
    list.removeValueAtBegin(&values);
    list.removeValueAtEnd(&values);
    list.removeValue(&values);
    CHECK((values == QList<int>{1, 20, 3}));
    CHECK((valuesOf<int>(list, &values) == QList<int>{1, 20, 3}));

    // Iterators: advance, compare, copy, difference, read, write, insert and erase.
    void *begin = list.begin(&values);
    void *it = list.begin(&values);
    void *end = list.end(&values);
    CHECK(list.diffIterator(end, begin) == 3);
    list.advanceIterator(it, 2);
    CHECK(list.diffIterator(it, begin) == 2 && !list.compareIterator(it, begin));
    list.valueAtIterator(it, &v);
    CHECK(v == 3);
    v = 30;
    list.setValueAtIterator(it, &v);
    list.advanceIterator(it, -1);
    list.valueAtIterator(it, &v);
    CHECK(v == 20);
    list.copyIterator(it, begin);
    CHECK(list.compareIterator(it, begin));
    list.destroyIterator(it);
    list.destroyIterator(begin);
    list.destroyIterator(end);
    CHECK((values == QList<int>{1, 20, 30}));
    it = list.begin(&values);
    list.advanceIterator(it, 1);
    v = 15;
    list.insertValueAtIterator(&values, it, &v); // invalidates the iterator
    list.destroyIterator(it);
    CHECK((values == QList<int>{1, 15, 20, 30}));
    it = list.begin(&values);
    list.eraseValueAtIterator(&values, it);
    list.destroyIterator(it);
    CHECK((values == QList<int>{15, 20, 30}));
    void *from = list.begin(&values);
    void *to = list.begin(&values);
    list.advanceIterator(to, 2);
    list.eraseRangeAtIterator(&values, from, to);
    list.destroyIterator(from);
    list.destroyIterator(to);
    CHECK((values == QList<int>{30}));
    list.clear(&values);
    CHECK(values.isEmpty());

    // A std::list has no random access, a std::vector cannot add at the front, a set cannot replace values.
    const QMetaSequence stdList = QMetaSequence::fromContainer<std::list<QString>>();
    CHECK(stdList.hasBidirectionalIterator() && !stdList.hasRandomAccessIterator() && !stdList.isSortable());
    CHECK(!stdList.canGetValueAtIndex() && stdList.canAddValueAtBegin() && stdList.canSetValueAtIterator());
    std::list<QString> names = {QStringLiteral("b")};
    const QString a = QStringLiteral("a");
    stdList.addValueAtBegin(&names, &a);
    CHECK((valuesOf<QString>(stdList, &names) == QList<QString>{QStringLiteral("a"), QStringLiteral("b")}));

    const QMetaSequence vector = QMetaSequence::fromContainer<std::vector<double>>();
    CHECK(!vector.canAddValueAtBegin() && vector.canAddValueAtEnd() && vector.canRemoveValueAtEnd() &&
          !vector.canRemoveValueAtBegin() && vector.canRemoveValue());
    std::vector<double> numbers;
    const double half = 0.5;
    vector.addValue(&numbers, &half);
    vector.addValueAtBegin(&numbers, &half); // not possible: nothing happens
    CHECK(numbers.size() == 1 && numbers[0] == 0.5);

    for (const QMetaSequence set :
         {QMetaSequence::fromContainer<QSet<int>>(), QMetaSequence::fromContainer<std::set<int>>()}) {
        CHECK(set.canAddValue() && !set.canAddValueAtEnd() && !set.canAddValueAtBegin());
        CHECK(!set.canSetValueAtIterator() && set.canGetValueAtIterator() && set.canEraseValueAtIterator());
        CHECK(!set.canRemoveValue() && set.hasSize());
    }
    QSet<int> set;
    const QMetaSequence qset = QMetaSequence::fromContainer<QSet<int>>();
    for (int i : {3, 1, 3})
        qset.addValue(&set, &i);
    CHECK(set.size() == 2 && set.contains(1) && set.contains(3));

    const QMetaSequence strings = QMetaSequence::fromContainer<QStringList>();
    CHECK(strings.valueMetaType() == QMetaType::fromType<QString>());

    // Values that cannot be assigned can still be counted, iterated and added, but not read out; a vector of them
    // cannot insert or erase in the middle, which moves values by assignment, but a list can.
    const QMetaSequence fixed = QMetaSequence::fromContainer<std::vector<Fixed>>();
    CHECK(fixed.hasSize() && fixed.canAddValueAtEnd() && !fixed.canEraseValueAtIterator());
    CHECK(!fixed.canGetValueAtIndex() && !fixed.canGetValueAtIterator() && !fixed.canSetValueAtIterator());
    CHECK(QMetaSequence::fromContainer<std::list<Fixed>>().canEraseValueAtIterator());
    std::vector<Fixed> fixedValues;
    const Fixed seven(7);
    fixed.addValueAtEnd(&fixedValues, &seven);
    CHECK(fixed.size(&fixedValues) == 1);

    // A meta-sequence of no type does nothing.
    const QMetaSequence none;
    CHECK(!none.hasSize() && !none.hasIterator() && !none.valueMetaType().isValid() && none.size(&values) == -1);
    CHECK(none.begin(&values) == nullptr);
}

static void testMetaAssociation() {
    const QMetaAssociation map = QMetaAssociation::fromContainer<QMap<QString, int>>();
    CHECK(map.keyMetaType() == QMetaType::fromType<QString>() && map.mappedMetaType() == QMetaType::fromType<int>());
    CHECK(map.canInsertKey() && map.canRemoveKey() && map.canContainsKey() && map.canGetMappedAtKey() &&
          map.canSetMappedAtKey());
    CHECK(map.canGetKeyAtIterator() && map.canGetKeyAtConstIterator() && map.canGetMappedAtIterator() &&
          map.canGetMappedAtConstIterator() && map.canSetMappedAtIterator());
    CHECK(map.canCreateIteratorAtKey() && map.canCreateConstIteratorAtKey() && map.hasBidirectionalIterator());
    QMap<QString, int> ages;
    const QString ada = QStringLiteral("Ada"), alan = QStringLiteral("Alan"), nobody = QStringLiteral("Nobody");
    int age = 36;
    map.setMappedAtKey(&ages, &ada, &age);
    map.insertKey(&ages, &alan);
    CHECK(ages.size() == 2 && ages.value(ada) == 36 && ages.contains(alan) && ages.value(alan) == 0);
    map.insertKey(&ages, &ada); // there already: unchanged
    CHECK(ages.value(ada) == 36);
    CHECK(map.containsKey(&ages, &ada) && !map.containsKey(&ages, &nobody));
    int found = -1;
    map.mappedAtKey(&ages, &ada, &found);
    CHECK(found == 36);
    map.mappedAtKey(&ages, &nobody, &found);
    CHECK(found == 0);

    void *it = map.createIteratorAtKey(&ages, &alan);
    QString key;
    map.keyAtIterator(it, &key);
    CHECK(key == alan);
    age = 41;
    map.setMappedAtIterator(it, &age);
    map.mappedAtIterator(it, &found);
    CHECK(found == 41 && ages.value(alan) == 41);
    map.destroyIterator(it);
    void *missing = map.createConstIteratorAtKey(&ages, &nobody);
    void *end = map.constEnd(&ages);
    CHECK(map.compareConstIterator(missing, end));
    map.destroyConstIterator(missing);
    map.destroyConstIterator(end);

    // Keys in order, through const iterators.
    QStringList keys;
    void *ci = map.constBegin(&ages);
    void *ce = map.constEnd(&ages);
    for (; !map.compareConstIterator(ci, ce); map.advanceConstIterator(ci, 1)) {
        map.keyAtConstIterator(ci, &key);
        keys.append(key);
    }
    map.destroyConstIterator(ci);
    map.destroyConstIterator(ce);
    CHECK((keys == QStringList{ada, alan}));

    map.removeKey(&ages, &ada);
    CHECK(ages.size() == 1 && !ages.contains(ada));

    // The same operations on the other associative containers.
    const QMetaAssociation hash = QMetaAssociation::fromContainer<QHash<int, QString>>();
    const QMetaAssociation stdMap = QMetaAssociation::fromContainer<std::map<int, QString>>();
    const QMetaAssociation unordered = QMetaAssociation::fromContainer<std::unordered_map<int, QString>>();
    CHECK(!unordered.hasBidirectionalIterator() && unordered.hasForwardIterator());
    QHash<int, QString> h;
    std::map<int, QString> m;
    std::unordered_map<int, QString> u;
    const int one = 1, two = 2;
    for (const auto &[meta, container] :
         {std::pair<QMetaAssociation, void *>{hash, &h}, {stdMap, &m}, {unordered, &u}}) {
        CHECK(meta.keyMetaType() == QMetaType::fromType<int>());
        meta.setMappedAtKey(container, &one, &ada);
        meta.insertKey(container, &two);
        CHECK(meta.size(container) == 2 && meta.containsKey(container, &one));
        QString mapped;
        meta.mappedAtKey(container, &one, &mapped);
        CHECK(mapped == ada);
        meta.removeKey(container, &two);
        CHECK(meta.size(container) == 1 && !meta.containsKey(container, &two));
        meta.clear(container);
        CHECK(meta.size(container) == 0);
    }

    // Mapped values that cannot be default-constructed cannot be read by key or inserted alone.
    const QMetaAssociation fixed = QMetaAssociation::fromContainer<std::map<int, Fixed>>();
    CHECK(fixed.canContainsKey() && fixed.canRemoveKey() && !fixed.canInsertKey() && !fixed.canGetMappedAtKey());
    CHECK(!QMetaAssociation().canInsertKey() && !QMetaAssociation().keyMetaType().isValid());

    // The iterables of a QVariant hand out the meta-container of the type it holds.
    const QVariant list = QVariant::fromValue(QList<int>{4, 5});
    const QSequentialIterable seq = list.value<QSequentialIterable>();
    CHECK(seq.metaContainer() == QMetaSequence::fromContainer<QList<int>>());
    CHECK(seq.metaContainer().size(list.constData()) == 2);
    const QVariant hashVariant = QVariant::fromValue(QHash<int, QString>{{1, ada}});
    const QAssociativeIterable assoc = hashVariant.value<QAssociativeIterable>();
    CHECK(assoc.metaContainer() == hash);
    CHECK(assoc.metaContainer().containsKey(hashVariant.constData(), &one));
    CHECK(QSequentialIterable(QVariant(42)).metaContainer() == QMetaSequence());
    CHECK(QAssociativeIterable(QVariant(42)).metaContainer() == QMetaAssociation());
}

static void testApplicationSettings() {
    // Attributes are flags that stay set until cleared.
    CHECK(!QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts));
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager, true);
    CHECK(QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts) &&
          QCoreApplication::testAttribute(Qt::AA_DisableSessionManager));
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
    CHECK(!QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts) &&
          QCoreApplication::testAttribute(Qt::AA_DisableSessionManager));
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager, false);

    CHECK(!QCoreApplication::isSetuidAllowed());
    QCoreApplication::setSetuidAllowed(true);
    CHECK(QCoreApplication::isSetuidAllowed());
    QCoreApplication::setSetuidAllowed(false);

    // The setters emit a change signal when the value changes, and only then.
    int names = 0, versions = 0, orgs = 0, domains = 0;
    QObject::connect(qApp, &QCoreApplication::applicationNameChanged, [&] { ++names; });
    QObject::connect(qApp, &QCoreApplication::applicationVersionChanged, [&] { ++versions; });
    QObject::connect(qApp, &QCoreApplication::organizationNameChanged, [&] { ++orgs; });
    QObject::connect(qApp, &QCoreApplication::organizationDomainChanged, [&] { ++domains; });
    QCoreApplication::setApplicationName(QStringLiteral("probe"));
    QCoreApplication::setApplicationName(QStringLiteral("probe"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.2"));
    QCoreApplication::setOrganizationName(QStringLiteral("Org"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.example"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.example"));
    CHECK(names == 1 && versions == 1 && orgs == 1 && domains == 1);

    // They are properties too.
    CHECK(qApp->property("applicationName").toString() == QStringLiteral("probe"));
    CHECK(qApp->setProperty("organizationName", QStringLiteral("Other")) && orgs == 2);
    CHECK(QCoreApplication::organizationName() == QStringLiteral("Other"));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testByteArrayView();
    testLatin1();
    testArg();
    testStringViewExtras();
    testSpanBytes();
    testVariantTypes();
    testVariantJson();
    testVariantCompare();
    testVariantInPlace();
    testUrl();
    testIoChannels();
    testVersionAndUuid();
    testStringListAndUninitialized();
    testPermissions();
    testYieldCpu();
    testMetaSequence();
    testMetaAssociation();
    testApplicationSettings();
    if (g_failures == 0)
        std::printf("all core API tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
