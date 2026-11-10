// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qmargins.h>
#include <QtCore/qset.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

struct Money {
    int cents = 0;
    bool operator==(const Money &o) const { return cents == o.cents; }
    bool operator<(const Money &o) const { return cents < o.cents; }
};
Q_DECLARE_METATYPE(Money)

static QDataStream &operator<<(QDataStream &s, const Money &m) { return s << static_cast<qint32>(m.cents); }
static QDataStream &operator>>(QDataStream &s, Money &m) {
    qint32 c = 0;
    s >> c;
    m.cents = c;
    return s;
}
static QDebug operator<<(QDebug d, const Money &m) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "Money(" << m.cents << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

// No operator==, no streaming.
struct Opaque {
    int v = 0;
};
Q_DECLARE_METATYPE(Opaque)

struct NoDefault {
    explicit NoDefault(int x) : v(x) {}
    int v;
};

namespace ns {
struct Inner {
    int a = 1;
};
} // namespace ns

class Plain : public QObject {
public:
    int tag = 0;
};

static QStringList g_messages;
static void collect(QtMsgType, const QMessageLogContext &, const QString &msg) { g_messages.append(msg); }

static QString debugText(const QVariant &v) {
    g_messages.clear();
    const QtMessageHandler old = qInstallMessageHandler(collect);
    qDebug() << v;
    qInstallMessageHandler(old);
    return g_messages.isEmpty() ? QString() : g_messages.last();
}

template <typename T> static QString debugOf(const T &value) {
    g_messages.clear();
    const QtMessageHandler old = qInstallMessageHandler(collect);
    qDebug() << value;
    qInstallMessageHandler(old);
    return g_messages.isEmpty() ? QString() : g_messages.last();
}

// QMetaType

static void testBuiltinIds() {
    CHECK(QMetaType::fromType<bool>().id() == QMetaType::Bool);
    CHECK(QMetaType::fromType<int>().id() == QMetaType::Int);
    CHECK(QMetaType::fromType<uint>().id() == QMetaType::UInt);
    CHECK(QMetaType::fromType<qlonglong>().id() == QMetaType::LongLong);
    CHECK(QMetaType::fromType<qulonglong>().id() == QMetaType::ULongLong);
    CHECK(QMetaType::fromType<double>().id() == QMetaType::Double);
    CHECK(QMetaType::fromType<float>().id() == QMetaType::Float);
    CHECK(QMetaType::fromType<long>().id() == QMetaType::Long);
    CHECK(QMetaType::fromType<ulong>().id() == QMetaType::ULong);
    CHECK(QMetaType::fromType<short>().id() == QMetaType::Short);
    CHECK(QMetaType::fromType<ushort>().id() == QMetaType::UShort);
    CHECK(QMetaType::fromType<char>().id() == QMetaType::Char);
    CHECK(QMetaType::fromType<uchar>().id() == QMetaType::UChar);
    CHECK(QMetaType::fromType<signed char>().id() == QMetaType::SChar);
    CHECK(QMetaType::fromType<char16_t>().id() == QMetaType::Char16);
    CHECK(QMetaType::fromType<char32_t>().id() == QMetaType::Char32);
    CHECK(QMetaType::fromType<std::nullptr_t>().id() == QMetaType::Nullptr);
    CHECK(QMetaType::fromType<void>().id() == QMetaType::Void);
    CHECK(QMetaType::fromType<void *>().id() == QMetaType::VoidStar);
    CHECK(QMetaType::fromType<QObject *>().id() == QMetaType::QObjectStar);
    CHECK(QMetaType::fromType<QString>().id() == QMetaType::QString);
    CHECK(QMetaType::fromType<QChar>().id() == QMetaType::QChar);
    CHECK(QMetaType::fromType<QByteArray>().id() == QMetaType::QByteArray);
    CHECK(QMetaType::fromType<QStringList>().id() == QMetaType::QStringList);
    CHECK(QMetaType::fromType<QByteArrayList>().id() == QMetaType::QByteArrayList);
    CHECK(QMetaType::fromType<QDate>().id() == QMetaType::QDate);
    CHECK(QMetaType::fromType<QTime>().id() == QMetaType::QTime);
    CHECK(QMetaType::fromType<QDateTime>().id() == QMetaType::QDateTime);
    CHECK(QMetaType::fromType<QUrl>().id() == QMetaType::QUrl);
    CHECK(QMetaType::fromType<QUuid>().id() == QMetaType::QUuid);
    CHECK(QMetaType::fromType<QBitArray>().id() == QMetaType::QBitArray);
    CHECK(QMetaType::fromType<QPoint>().id() == QMetaType::QPoint);
    CHECK(QMetaType::fromType<QPointF>().id() == QMetaType::QPointF);
    CHECK(QMetaType::fromType<QSize>().id() == QMetaType::QSize);
    CHECK(QMetaType::fromType<QSizeF>().id() == QMetaType::QSizeF);
    CHECK(QMetaType::fromType<QLine>().id() == QMetaType::QLine);
    CHECK(QMetaType::fromType<QLineF>().id() == QMetaType::QLineF);
    CHECK(QMetaType::fromType<QRect>().id() == QMetaType::QRect);
    CHECK(QMetaType::fromType<QRectF>().id() == QMetaType::QRectF);
    CHECK(QMetaType::fromType<QVariant>().id() == QMetaType::QVariant);
    CHECK(QMetaType::fromType<QVariantList>().id() == QMetaType::QVariantList);
    CHECK(QMetaType::fromType<QVariantMap>().id() == QMetaType::QVariantMap);
    CHECK(QMetaType::fromType<QVariantHash>().id() == QMetaType::QVariantHash);
    CHECK(QMetaType::fromType<QVariantPair>().id() == QMetaType::QVariantPair);
    CHECK(QMetaType::fromType<const int>().id() == QMetaType::Int);
    CHECK(QMetaType::fromType<int &>().id() == QMetaType::Int);
    // The numbers are those of Qt.
    CHECK(QMetaType::Bool == 1 && QMetaType::Int == 2 && QMetaType::QString == 10 && QMetaType::QByteArray == 12);
    CHECK(QMetaType::QUuid == 30 && QMetaType::Float == 38 && QMetaType::QObjectStar == 39 && QMetaType::User == 65536);

    CHECK(std::string(QMetaType::typeName(QMetaType::Int)) == "int");
    CHECK(std::string(QMetaType::typeName(QMetaType::LongLong)) == "qlonglong");
    CHECK(std::string(QMetaType::typeName(QMetaType::QString)) == "QString");
    CHECK(std::string(QMetaType::typeName(QMetaType::VoidStar)) == "void*");
    CHECK(std::string(QMetaType::typeName(QMetaType::QVariantMap)) == "QVariantMap");
    CHECK(QMetaType::typeName(QMetaType::UnknownType) == nullptr);
    CHECK(QMetaType::typeName(123456789) == nullptr);
    CHECK(std::string(QMetaType::fromType<unsigned>().name()) == "uint");
}

static void testFromName() {
    CHECK(QMetaType::type("int") == QMetaType::Int);
    CHECK(QMetaType::type("unsigned int") == QMetaType::UInt);
    CHECK(QMetaType::type("uint") == QMetaType::UInt);
    CHECK(QMetaType::type("unsigned long long") == QMetaType::ULongLong);
    CHECK(QMetaType::type("qlonglong") == QMetaType::LongLong);
    CHECK(QMetaType::type("long long") == QMetaType::LongLong);
    CHECK(QMetaType::type("qint8") == QMetaType::SChar);
    CHECK(QMetaType::type("quint16") == QMetaType::UShort);
    CHECK(QMetaType::type("qreal") == QMetaType::Double);
    CHECK(QMetaType::type("QList<QVariant>") == QMetaType::QVariantList);
    CHECK(QMetaType::type("QMap<QString, QVariant>") == QMetaType::QVariantMap);
    CHECK(QMetaType::type("QObject *") == QMetaType::QObjectStar);
    CHECK(QMetaType::type(QByteArray("QString")) == QMetaType::QString);
    CHECK(QMetaType::fromName("QDate") == QMetaType::fromType<QDate>());
    CHECK(!QMetaType::fromName("NoSuchType").isValid());
    CHECK(!QMetaType::fromName("").isValid());
    CHECK(QMetaType::type("NoSuchType") == QMetaType::UnknownType);
    CHECK(QMetaType::type(nullptr) == QMetaType::UnknownType);
    CHECK(QMetaType::isRegistered(QMetaType::Int));
    CHECK(!QMetaType::isRegistered(0));
    CHECK(!QMetaType::isRegistered(-5));
    CHECK(!QMetaType::isRegistered(QMetaType::QLocale)); // reserved, not implemented
    CHECK(std::string(QMetaType::typeName(QMetaType::QLocale)) == "QLocale");
    CHECK(!QMetaType(QMetaType::QCborValue).isValid());
    CHECK(!QMetaType().isValid());
    CHECK(QMetaType().id() == 0 && QMetaType().name() == nullptr && QMetaType().sizeOf() == 0);
    CHECK(!QMetaType(999999).isValid());
}

static void testTypeProperties() {
    const QMetaType i = QMetaType::fromType<int>();
    CHECK(i.sizeOf() == 4 && i.alignOf() == 4);
    CHECK(QMetaType::sizeOf(QMetaType::Double) == 8);
    CHECK(QMetaType::fromType<QString>().sizeOf() == static_cast<qsizetype>(sizeof(QString)));
    CHECK(i.isValid() && i.isRegistered());
    CHECK(!i.flags().testFlag(QMetaType::NeedsDestruction));
    CHECK(i.flags().testFlag(QMetaType::RelocatableType));
    CHECK(QMetaType::fromType<QString>().flags().testFlag(QMetaType::NeedsDestruction));
    CHECK(QMetaType::typeFlags(QMetaType::QString).testFlag(QMetaType::NeedsConstruction));
    CHECK(QMetaType::fromType<QObject *>().flags().testFlag(QMetaType::PointerToQObject));
    CHECK(!QMetaType::fromType<void *>().flags().testFlag(QMetaType::PointerToQObject));
    CHECK(QMetaType::fromType<Plain *>().flags().testFlag(QMetaType::PointerToQObject));
    CHECK(i.isDefaultConstructible() && i.isCopyConstructible() && i.isMoveConstructible() && i.isDestructible());
    CHECK(i.isEqualityComparable() && i.isOrdered());
    CHECK(QMetaType::fromType<Opaque>().isDefaultConstructible());
    CHECK(!QMetaType::fromType<Opaque>().isEqualityComparable());
    CHECK(!QMetaType::fromType<Opaque>().isOrdered());
    CHECK(!QMetaType::fromType<NoDefault>().isDefaultConstructible());
    CHECK(QMetaType::fromType<NoDefault>().isCopyConstructible());
    CHECK(QMetaType::fromType<Money>().isOrdered());
    CHECK(QMetaType::fromType<QString>().isOrdered());
    CHECK(QMetaType::fromType<QStringList>().isEqualityComparable());
    CHECK(QMetaType::fromType<QVariantList>().isEqualityComparable());
    CHECK(!QMetaType::fromType<QList<Opaque>>().isEqualityComparable());
    CHECK(QMetaType::fromType<QList<int>>().isEqualityComparable());
    CHECK((QMetaType::fromType<QMap<QString, int>>().isEqualityComparable()));
    CHECK((!QMetaType::fromType<QMap<QString, Opaque>>().isEqualityComparable()));
}

static void testConstructDestroy() {
    const QMetaType t = QMetaType::fromType<QString>();
    void *p = t.create();
    CHECK(p && static_cast<QString *>(p)->isEmpty());
    t.destroy(p);
    const QString src = "copy me";
    p = t.create(&src);
    CHECK(p && *static_cast<QString *>(p) == "copy me");
    t.destroy(p);
    alignas(QString) unsigned char buf[sizeof(QString)];
    CHECK(t.construct(buf, &src) == buf);
    CHECK(*reinterpret_cast<QString *>(buf) == "copy me");
    t.destruct(buf);
    CHECK(t.construct(buf) == buf);
    t.destruct(buf);
    CHECK(t.construct(nullptr) == nullptr);
    void *q = QMetaType::create(QMetaType::Int);
    CHECK(q != nullptr);
    QMetaType::destroy(QMetaType::Int, q);
    int i = 5;
    q = QMetaType::create(QMetaType::Int, &i);
    CHECK(*static_cast<int *>(q) == 5);
    QMetaType::destroy(QMetaType::Int, q);
    CHECK(QMetaType::create(0) == nullptr);
    CHECK(QMetaType::fromType<NoDefault>().create() == nullptr);
    NoDefault nd(3);
    void *c = QMetaType::fromType<NoDefault>().create(&nd);
    CHECK(c && static_cast<NoDefault *>(c)->v == 3);
    QMetaType::fromType<NoDefault>().destroy(c);
    CHECK(QMetaType().create() == nullptr);
    QMetaType().destroy(nullptr);
    QMetaType().destruct(nullptr);
    // A type with alignment beyond the default
    struct alignas(64) Wide {
        char c = 0;
    };
    const QMetaType w = QMetaType::fromType<Wide>();
    CHECK(w.alignOf() == 64);
    void *wp = w.create();
    CHECK(reinterpret_cast<quintptr>(wp) % 64 == 0);
    w.destroy(wp);
    QVariant wv = QVariant::fromValue(Wide());
    CHECK(reinterpret_cast<quintptr>(wv.constData()) % 64 == 0);
}

static void testCompare() {
    const QMetaType t = QMetaType::fromType<int>();
    int a = 1, b = 2, a2 = 1;
    CHECK(t.equals(&a, &a2) && !t.equals(&a, &b));
    CHECK(QMetaType::equals(&a, &a2, QMetaType::Int));
    int r = 99;
    CHECK(t.compare(&a, &b, r) && r < 0);
    CHECK(t.compare(&b, &a, r) && r > 0);
    CHECK(t.compare(&a, &a2, r) && r == 0);
    Opaque o1, o2;
    const QMetaType ot = QMetaType::fromType<Opaque>();
    CHECK(!ot.equals(&o1, &o2)); // no operator==: only the same object is equal
    CHECK(ot.equals(&o1, &o1));
    CHECK(!ot.compare(&o1, &o2, r));
    CHECK(!QMetaType().equals(&a, &a));
    Money m1{1}, m2{2};
    CHECK(QMetaType::fromType<Money>().equals(&m1, &m1) && !QMetaType::fromType<Money>().equals(&m1, &m2));
    CHECK(QMetaType::fromType<Money>().compare(&m1, &m2, r) && r < 0);
}

// Registration

static void testRegistration() {
    const QMetaType money = QMetaType::fromType<Money>();
    const int id = money.id();
    CHECK(id >= QMetaType::User);
    CHECK(money.id() == id);
    CHECK(money.isRegistered());
    CHECK(std::string(money.name()) == "Money");
    CHECK(QMetaType::fromName("Money") == money);
    CHECK(QMetaType::type("Money") == id);
    CHECK(QMetaType(id) == money);
    CHECK(std::string(QMetaType::typeName(id)) == "Money");
    CHECK(qMetaTypeId<Money>() == id);
    CHECK(qRegisterMetaType<Money>() == id);
    CHECK(QMetaTypeId<Money>::qt_metatype_id() == id);

    // An alias
    CHECK(qRegisterMetaType<Money>("Cash") == id);
    CHECK(QMetaType::fromName("Cash") == money);
    CHECK(std::string(money.name()) == "Money");
    CHECK(QMetaType::registerNormalizedTypedef("Coins", money) == id);
    CHECK(QMetaType::type("Coins") == id);
    // A name that belongs to another type keeps its owner.
    CHECK(QMetaType::registerNormalizedTypedef("int", money) == QMetaType::Int);
    CHECK(QMetaType::registerNormalizedTypedef("Money", QMetaType::fromType<Opaque>()) == id);
    CHECK(QMetaType::registerNormalizedTypedef("X", QMetaType()) == 0);

    // A type without Q_DECLARE_METATYPE is named by the compiler.
    const QMetaType inner = QMetaType::fromType<ns::Inner>();
    CHECK(std::string(inner.name()) == "ns::Inner");
    CHECK(inner.id() >= QMetaType::User);
    CHECK(QMetaType::fromName("ns::Inner") == inner);
    CHECK(qRegisterMetaType<ns::Inner>("Inner") == inner.id());
    CHECK(QMetaType::fromName("Inner") == inner);
    // A container
    const QMetaType list = QMetaType::fromType<QList<Money>>();
    CHECK(std::string(list.name()) == "QList<Money>");
    CHECK(list.id() > id);
    CHECK(QMetaType::fromName("QList< Money >") == list);
    CHECK(qMetaTypeId<QList<Money>>() == list.id());
    // Distinct types get distinct ids.
    CHECK(QMetaType::fromType<Opaque>().id() != id);
    CHECK(QMetaType::fromType<std::vector<int>>().id() != QMetaType::fromType<std::deque<int>>().id());
}

// Conversion with registered functions

struct Celsius {
    double deg = 0;
};
struct Fahrenheit {
    double deg = 0;
    double toDouble() const { return deg; }
    QString toText(bool *ok) const {
        *ok = deg > -1000;
        return QString::number(deg) + "F";
    }
};
Q_DECLARE_METATYPE(Celsius)
Q_DECLARE_METATYPE(Fahrenheit)

static void testRegisteredConverters() {
    CHECK(!QMetaType::canConvert(QMetaType::fromType<Celsius>(), QMetaType::fromType<Fahrenheit>()));
    CHECK((QMetaType::registerConverter<Celsius, Fahrenheit>(
        [](const Celsius &c) { return Fahrenheit{c.deg * 9 / 5 + 32}; })));
    CHECK(QMetaType::canConvert(QMetaType::fromType<Celsius>(), QMetaType::fromType<Fahrenheit>()));
    QVariant c = QVariant::fromValue(Celsius{100});
    CHECK(c.canConvert<Fahrenheit>());
    CHECK(c.value<Fahrenheit>().deg == 212);
    CHECK(c.convert(QMetaType::fromType<Fahrenheit>()));
    CHECK(c.metaType() == QMetaType::fromType<Fahrenheit>());
    CHECK(QMetaType::registerConverter(&Fahrenheit::toDouble));
    CHECK(QVariant::fromValue(Fahrenheit{3.5}).toDouble() == 3.5);
    CHECK(QMetaType::registerConverter(&Fahrenheit::toText));
    CHECK(QVariant::fromValue(Fahrenheit{3.5}).toString() == "3.5F");
    bool ok = true;
    CHECK(QVariant::fromValue(Fahrenheit{-2000}).toString().isEmpty());
    (void)ok;
    // An implicit conversion
    struct Meters {
        double m;
    };
    struct Feet {
        Feet() = default;
        Feet(const Meters &m) : f(m.m * 3.28084) {}
        double f = 0;
    };
    CHECK((QMetaType::registerConverter<Meters, Feet>()));
    CHECK(std::fabs(QVariant::fromValue(Meters{10}).value<Feet>().f - 32.8084) < 1e-6);
    // Removing a conversion
    QMetaType::unregisterConverterFunction(QMetaType::fromType<Celsius>(), QMetaType::fromType<Fahrenheit>());
    CHECK(!QVariant::fromValue(Celsius{1}).canConvert<Fahrenheit>());
    // A registered conversion overrides a built-in one.
    QMetaType::registerConverterFunction(
        [](const void *, void *to) {
            *static_cast<QString *>(to) = "always";
            return true;
        },
        QMetaType::fromType<Money>(), QMetaType::fromType<QString>());
    CHECK(QVariant::fromValue(Money{5}).toString() == "always");
    QMetaType::unregisterConverterFunction(QMetaType::fromType<Money>(), QMetaType::fromType<QString>());
    CHECK(QVariant::fromValue(Money{5}).toString().isEmpty());
    CHECK(!QMetaType::registerConverterFunction({}, QMetaType(), QMetaType::fromType<int>()));
}

// QVariant construction

static void testConstruction() {
    CHECK(!QVariant().isValid() && QVariant().isNull() && QVariant().typeId() == QMetaType::UnknownType);
    CHECK(QVariant().typeName() == nullptr);
    CHECK(QVariant().type() == QVariant::Invalid);
    CHECK(QVariant(1).typeId() == QMetaType::Int && QVariant(1).type() == QVariant::Int);
    CHECK(QVariant(1u).typeId() == QMetaType::UInt);
    CHECK(QVariant(qlonglong(1)).typeId() == QMetaType::LongLong);
    CHECK(QVariant(qulonglong(1)).typeId() == QMetaType::ULongLong);
    CHECK(QVariant(qint64(1)).typeId() == QMetaType::LongLong); // long on Linux
    CHECK(QVariant(quint64(1)).typeId() == QMetaType::ULongLong);
    CHECK(QVariant(1L).typeId() == QMetaType::LongLong);
    CHECK(QVariant(1UL).typeId() == QMetaType::ULongLong);
    CHECK(QVariant(true).typeId() == QMetaType::Bool);
    CHECK(QVariant(1.5).typeId() == QMetaType::Double);
    CHECK(QVariant(1.5f).typeId() == QMetaType::Float);
    CHECK(QVariant("text").typeId() == QMetaType::QString);
    CHECK(QVariant("text").toString() == "text");
    CHECK(QVariant(static_cast<const char *>(nullptr)).isNull());
    CHECK(QVariant(QLatin1StringView("abc")).toString() == "abc");
    CHECK(QVariant(QByteArray("b")).typeId() == QMetaType::QByteArray);
    CHECK(QVariant(QString("s")).typeId() == QMetaType::QString);
    CHECK(QVariant(QChar('c')).typeId() == QMetaType::QChar);
    CHECK(QVariant(QStringList{"a"}).typeId() == QMetaType::QStringList);
    CHECK(QVariant(nullptr).typeId() == QMetaType::Nullptr);
    CHECK(QVariant(QDate(2020, 1, 2)).typeId() == QMetaType::QDate);
    CHECK(QVariant(QTime(1, 2, 3)).typeId() == QMetaType::QTime);
    CHECK(QVariant(QDateTime(QDate(2020, 1, 2), QTime(3, 4, 5), Qt::UTC)).typeId() == QMetaType::QDateTime);
    CHECK(QVariant(QVariantList{1}).typeId() == QMetaType::QVariantList);
    CHECK(QVariant(QVariantMap{{"a", 1}}).typeId() == QMetaType::QVariantMap);
    CHECK(QVariant(QVariantHash{{"a", 1}}).typeId() == QMetaType::QVariantHash);
    CHECK(QVariant(QSize(1, 2)).typeId() == QMetaType::QSize);
    CHECK(QVariant(QSizeF(1, 2)).typeId() == QMetaType::QSizeF);
    CHECK(QVariant(QPoint(1, 2)).typeId() == QMetaType::QPoint);
    CHECK(QVariant(QPointF(1, 2)).typeId() == QMetaType::QPointF);
    CHECK(QVariant(QLine(1, 2, 3, 4)).typeId() == QMetaType::QLine);
    CHECK(QVariant(QLineF(1, 2, 3, 4)).typeId() == QMetaType::QLineF);
    CHECK(QVariant(QRect(1, 2, 3, 4)).typeId() == QMetaType::QRect);
    CHECK(QVariant(QRectF(1, 2, 3, 4)).typeId() == QMetaType::QRectF);
    CHECK(QVariant(QUrl("http://x")).typeId() == QMetaType::QUrl);
    CHECK(QVariant(QBitArray(3)).typeId() == QMetaType::QBitArray);
    CHECK(QVariant(QMetaType::fromType<int>()).typeId() == QMetaType::Int);
    CHECK(QVariant(QMetaType::fromType<int>()).isNull());
    CHECK(QVariant(QMetaType::fromType<int>()).toInt() == 0);
    int five = 5;
    CHECK(!QVariant(QMetaType::fromType<int>(), &five).isNull());
    CHECK(QVariant(QMetaType::fromType<int>(), &five).toInt() == 5);
    CHECK(!QVariant(QMetaType()).isValid());
    CHECK(!QVariant(QMetaType::fromType<NoDefault>()).isValid()); // cannot be default-constructed
    CHECK(!QVariant(QMetaType::fromType<void>()).isValid());
    CHECK(QVariant::fromMetaType(QMetaType::fromType<QString>()).typeId() == QMetaType::QString);
    // Types with their own id
    CHECK(QVariant::fromValue(QUuid()).typeId() == QMetaType::QUuid);
    CHECK(QVariant::fromValue(QVersionNumber(1, 2)).typeId() >= QMetaType::User);
    CHECK(QVariant::fromValue(QVersionNumber(1, 2)).type() == QVariant::UserType);
    CHECK(std::string(QVariant::fromValue(QVersionNumber(1, 2)).typeName()) == "QVersionNumber");
    CHECK(QVariant::fromValue(Money{3}).userType() == qMetaTypeId<Money>());
    CHECK(QVariant::typeToName(QMetaType::Int) == std::string("int"));
    CHECK(QVariant::typeToName(0) == nullptr);
    CHECK(QVariant::nameToType("QString") == QVariant::String);
    CHECK(QVariant::nameToType("Invalid") == QVariant::Invalid);
    CHECK(QVariant::nameToType("") == QVariant::Invalid);
    CHECK(QVariant::nameToType(nullptr) == QVariant::Invalid);
    CHECK(QVariant::nameToType("Money") == QVariant::UserType);
    CHECK(QVariant::nameToType("nonsense") == QVariant::Invalid);
}

static void testNullAndClear() {
    CHECK(QVariant(QString()).isNull());
    CHECK(QVariant(QString()).isValid());
    CHECK(!QVariant(QString("")).isNull());
    CHECK(QVariant(QByteArray()).isNull());
    CHECK(!QVariant(QByteArray("")).isNull());
    CHECK(QVariant(QDate()).isNull());
    CHECK(!QVariant(QDate(2020, 1, 1)).isNull());
    CHECK(QVariant(QTime()).isNull());
    CHECK(QVariant(QDateTime()).isNull());
    CHECK(QVariant(QChar()).isNull());
    CHECK(!QVariant(0).isNull());
    CHECK(!QVariant(QStringList()).isNull());
    CHECK(!QVariant(QVariantList()).isNull());
    CHECK(QVariant(QSize()).isNull() == QSize().isNull());
    CHECK(QVariant(QPoint()).isNull() && !QVariant(QPoint(1, 0)).isNull());
    CHECK(QVariant(QRect()).isNull());
    CHECK(QVariant(QLine()).isNull());
    CHECK(QVariant::fromValue(QUuid()).isNull());
    CHECK(!QVariant::fromValue(QUuid::createUuid()).isNull());
    CHECK(QVariant::fromValue(static_cast<QObject *>(nullptr)).isNull());
    QObject o;
    CHECK(!QVariant::fromValue(&o).isNull());
    CHECK(QVariant::fromValue(static_cast<Plain *>(nullptr)).isNull());
    CHECK(QVariant::fromValue(static_cast<int *>(nullptr)).isNull());
    int x = 0;
    CHECK(!QVariant::fromValue(&x).isNull());
    CHECK(QVariant(nullptr).isNull());

    QVariant v = 5;
    v.clear();
    CHECK(!v.isValid() && v.isNull());
    QVariant a = 1, b = QString("b");
    a.swap(b);
    CHECK(a.toString() == "b" && b.toInt() == 1);
    QVariant moved = std::move(a);
    CHECK(moved.toString() == "b");
    QVariant copy = moved;
    CHECK(copy == moved);
    copy = 7;
    CHECK(copy.toInt() == 7 && moved.toString() == "b");
}

static void testSharedStorage() {
    QVariant a = QString("shared");
    QVariant b = a;
    CHECK(a.constData() == b.constData()); // shared until written
    *static_cast<QString *>(b.data()) = "changed";
    CHECK(a.toString() == "shared" && b.toString() == "changed");
    CHECK(a.constData() != b.constData());
    // data() clears the null flag
    QVariant n(QMetaType::fromType<int>());
    CHECK(n.isNull());
    *static_cast<int *>(n.data()) = 4;
    CHECK(!n.isNull() && n.toInt() == 4);
    CHECK(QVariant().data() == nullptr && QVariant().constData() == nullptr);
    const QVariant c = 3;
    CHECK(*static_cast<const int *>(c.data()) == 3);
}

// Conversions

static void testNumericConversions() {
    bool ok = false;
    CHECK(QVariant(42).toInt(&ok) == 42 && ok);
    CHECK(QVariant(42).toUInt() == 42u);
    CHECK(QVariant(-1).toLongLong() == -1);
    CHECK(QVariant(-1).toUInt(&ok) == 0xffffffffu && ok);
    CHECK(QVariant(3.7).toInt() == 4);
    CHECK(QVariant(-3.7).toInt() == -4);
    CHECK(QVariant(3.2f).toInt() == 3);
    CHECK(QVariant(1e30).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(std::numeric_limits<double>::quiet_NaN()).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(std::numeric_limits<double>::infinity()).toLongLong(&ok) == 0 && !ok);
    CHECK(QVariant(-1e30).toLongLong(&ok) == 0 && !ok);
    CHECK(QVariant(2147483647.4).toInt(&ok) == 2147483647 && ok);
    CHECK(QVariant(2147483648.0).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(qulonglong(18446744073709551615ULL)).toULongLong(&ok) == 18446744073709551615ULL && ok);
    CHECK(QVariant(1.8446744073709552e19).toULongLong(&ok) == 0 && !ok);
    CHECK(QVariant(true).toInt() == 1 && QVariant(false).toInt() == 0);
    CHECK(QVariant(5).toBool() && !QVariant(0).toBool());
    CHECK(QVariant(0.5).toBool() && !QVariant(0.0).toBool());
    CHECK(QVariant(5).toDouble() == 5.0 && QVariant(5).toFloat() == 5.0f && QVariant(5).toReal() == 5.0);
    CHECK(QVariant(qulonglong(1) << 63).toDouble() == 9223372036854775808.0);
    CHECK(QVariant(QChar('A')).toInt() == 65);
    CHECK(QVariant('A').toInt() == 65); // char
    CHECK(QVariant(65).toChar() == QChar('A'));
    CHECK(QVariant(70000).toChar().isNull()); // not a UTF-16 unit
    CHECK(QVariant::fromValue<short>(-5).toInt() == -5);
    CHECK(QVariant::fromValue<ushort>(65535).toInt() == 65535);
    CHECK(QVariant::fromValue<uchar>(255).toInt() == 255);
    CHECK(QVariant::fromValue<signed char>(-3).toInt() == -3);
    CHECK(QVariant::fromValue<ulong>(7).toInt() == 7);
    CHECK(QVariant::fromValue<long>(-7).toInt() == -7);
    CHECK(QVariant::fromValue<char16_t>(u'x').toInt() == 'x');
    CHECK(QVariant::fromValue<char32_t>(U'x').toInt() == 'x');
    CHECK(QVariant::fromValue<float>(0.5f).toDouble() == 0.5);
    CHECK(QVariant(1).toInt(nullptr) == 1);
    CHECK(!QVariant().toInt(&ok) && !ok);
    CHECK(QVariant().toDouble(&ok) == 0.0 && !ok);
    CHECK(QVariant(QDate(2020, 1, 2)).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(QStringList{"1"}).toInt(&ok) == 0 && !ok);
}

static void testStringConversions() {
    bool ok = false;
    CHECK(QVariant(QString("12")).toInt(&ok) == 12 && ok);
    CHECK(QVariant(QString(" 12 ")).toInt(&ok) == 12 && ok);
    CHECK(QVariant(QString("-12")).toInt() == -12);
    CHECK(QVariant(QString("12abc")).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(QString("")).toInt(&ok) == 0 && !ok);
    CHECK(QVariant(QString("3.7")).toInt(&ok) == 4 && ok);
    CHECK(QVariant(QString("1e3")).toInt(&ok) == 1000 && ok);
    CHECK(QVariant(QString("4294967295")).toUInt(&ok) == 4294967295u && ok);
    CHECK(QVariant(QString("-1")).toUInt() == 0xffffffffu);
    CHECK(QVariant(QString("9223372036854775807")).toLongLong() == std::numeric_limits<qlonglong>::max());
    CHECK(QVariant(QString("18446744073709551615")).toULongLong(&ok) == std::numeric_limits<qulonglong>::max() && ok);
    CHECK(QVariant(QString("2.5")).toDouble(&ok) == 2.5 && ok);
    CHECK(QVariant(QString("nope")).toDouble(&ok) == 0 && !ok);
    CHECK(QVariant(QString("1.5")).toFloat() == 1.5f);
    CHECK(QVariant(QByteArray("77")).toInt() == 77);
    CHECK(QVariant(QByteArray("2.5")).toDouble() == 2.5);
    // bool
    CHECK(QVariant(QString("true")).toBool() && QVariant(QString("TRUE")).toBool() &&
          QVariant(QString("yes")).toBool());
    CHECK(!QVariant(QString("false")).toBool() && !QVariant(QString("False")).toBool());
    CHECK(!QVariant(QString("0")).toBool() && !QVariant(QString("")).toBool());
    CHECK(QVariant(QString("1")).toBool() && QVariant(QString("anything")).toBool());
    CHECK(!QVariant(QByteArray("false")).toBool() && QVariant(QByteArray("x")).toBool());
    CHECK(!QVariant().toBool());
    // to string
    CHECK(QVariant(5).toString() == "5" && QVariant(-5).toString() == "-5");
    CHECK(QVariant(5u).toString() == "5");
    CHECK(QVariant(qulonglong(18446744073709551615ULL)).toString() == "18446744073709551615");
    CHECK(QVariant(true).toString() == "true" && QVariant(false).toString() == "false");
    CHECK(QVariant(1.5).toString() == "1.5");
    CHECK(QVariant(0.1).toString() == "0.1");
    CHECK(QVariant(1.0).toString() == "1");
    CHECK(QVariant(1e20).toString() == "1e+20");
    CHECK(QVariant(0.1f).toString() == "0.1");
    CHECK(QVariant(1.0f / 3.0f).toString() == "0.33333334");
    CHECK(QVariant(1.0 / 3.0).toString() == "0.3333333333333333");
    CHECK(QVariant(std::numeric_limits<double>::infinity()).toString() == "inf");
    CHECK(QVariant(-std::numeric_limits<double>::infinity()).toString() == "-inf");
    CHECK(QVariant(std::numeric_limits<double>::quiet_NaN()).toString() == "nan");
    CHECK(QVariant(QChar('x')).toString() == "x");
    CHECK(QVariant(QByteArray("bytes")).toString() == "bytes");
    CHECK(QVariant(QByteArray("\xc3\xa9")).toString() == QString::fromUtf8("\xc3\xa9"));
    CHECK(QVariant(QStringList{"only"}).toString() == "only");
    CHECK(QVariant(QStringList{"a", "b"}).toString().isNull());
    CHECK(QVariant(QDate(2024, 2, 29)).toString() == "2024-02-29");
    CHECK(QVariant(QTime(1, 2, 3, 4)).toString() == "01:02:03.004");
    CHECK(QVariant(QDateTime(QDate(2024, 2, 29), QTime(1, 2, 3), Qt::UTC)).toString() == "2024-02-29T01:02:03.000Z");
    CHECK(QVariant(QUrl("http://example.com/p")).toString() == "http://example.com/p");
    const QUuid id = QUuid::createUuid();
    CHECK(QVariant::fromValue(id).toString() == id.toString());
    CHECK(QVariant::fromValue(QVersionNumber(1, 2, 3)).toString() == "1.2.3");
    CHECK(QVariant().toString().isNull());
    CHECK(QVariant(QPoint(1, 2)).toString().isNull());
    CHECK(QVariant(QVariantList{1}).toString().isNull());
    // to byte array
    CHECK(QVariant(QString("é")).toByteArray() == QString("é").toUtf8());
    CHECK(QVariant(12).toByteArray() == "12");
    CHECK(QVariant(true).toByteArray() == "true");
    CHECK(QVariant(1.5).toByteArray() == "1.5");
    CHECK(QVariant::fromValue(id).toByteArray() == id.toByteArray());
    CHECK(QVariant().toByteArray().isNull());
    // to char
    CHECK(QVariant(QString("x")).toChar() == QChar('x'));
    CHECK(QVariant(QString("xy")).toChar().isNull());
    CHECK(QVariant(QString("")).toChar().isNull());
}

static void testDateConversions() {
    const QDate d(2024, 2, 29);
    const QTime t(13, 14, 15, 160);
    const QDateTime dt(d, t, Qt::UTC);
    CHECK(QVariant(dt).toDate() == d);
    CHECK(QVariant(dt).toTime() == t);
    CHECK(QVariant(d).toDateTime().date() == d);
    CHECK(QVariant(d).toDateTime().time() == QTime(0, 0));
    CHECK(QVariant(QString("2024-02-29")).toDate() == d);
    CHECK(!QVariant(QString("garbage")).toDate().isValid());
    CHECK(QVariant(QString("13:14:15")).toTime() == QTime(13, 14, 15));
    CHECK(!QVariant(QString("garbage")).toTime().isValid());
    CHECK(QVariant(QString("2024-02-29T13:14:15Z")).toDateTime() == QDateTime(d, QTime(13, 14, 15), Qt::UTC));
    CHECK(!QVariant(QString("garbage")).toDateTime().isValid());
    CHECK(!QVariant(5).toDate().isValid());
    CHECK(QVariant(QDateTime()).toDate().isNull());
    CHECK(QVariant(QTime(1, 2)).toDate().isNull());
}

static void testStructuredConversions() {
    const QUrl url("http://example.com/a?b=1");
    CHECK(QVariant(QString("http://example.com/a?b=1")).toUrl() == url);
    CHECK(QVariant(url).toUrl() == url);
    CHECK(QVariant(5).toUrl().isEmpty());
    // Points and friends
    CHECK(QVariant(QPointF(1.4, 2.6)).toPoint() == QPoint(1, 3));
    CHECK(QVariant(QPoint(1, 2)).toPointF() == QPointF(1, 2));
    CHECK(QVariant(QSizeF(1.4, 2.6)).toSize() == QSize(1, 3));
    CHECK(QVariant(QSize(1, 2)).toSizeF() == QSizeF(1, 2));
    CHECK(QVariant(QLineF(1, 2, 3, 4)).toLine() == QLine(1, 2, 3, 4));
    CHECK(QVariant(QLine(1, 2, 3, 4)).toLineF() == QLineF(1, 2, 3, 4));
    CHECK(QVariant(QRectF(1, 2, 3, 4)).toRect() == QRect(1, 2, 3, 4));
    CHECK(QVariant(QRect(1, 2, 3, 4)).toRectF() == QRectF(1, 2, 3, 4));
    CHECK(QVariant(QPoint(1, 2)).value<QPoint>() == QPoint(1, 2));
    CHECK(QVariant(QSize(1, 2)).value<QSize>() == QSize(1, 2));
    CHECK(QVariant(QPoint(1, 2)).toSize() == QSize());
    CHECK(QVariant(7).toPoint() == QPoint());
    CHECK(!QVariant(QPoint(1, 2)).canConvert<QSize>());
    CHECK(QVariant(QPoint(1, 2)).canConvert<QPointF>());
    // QUuid and QVersionNumber from strings
    const QUuid id = QUuid::createUuid();
    CHECK(QVariant(id.toString()).value<QUuid>() == id);
    CHECK(QVariant(id.toString()).canConvert<QUuid>());
    CHECK(QVariant(QString("{00000000-0000-0000-0000-000000000000}")).value<QUuid>().isNull());
    CHECK(QVariant(QString("bogus")).value<QUuid>().isNull());
    QVariant bogus = QString("bogus");
    CHECK(!bogus.convert(QMetaType::fromType<QUuid>()));
    CHECK(QVariant(QString("1.2.3")).value<QVersionNumber>() == QVersionNumber(1, 2, 3));
    CHECK(QVariant(QString("")).value<QVersionNumber>().isNull());
    // lists
    CHECK((QVariant(QStringList{"a", "b"}).toList() == QVariantList{"a", "b"}));
    CHECK((QVariant::fromValue(QByteArrayList{"a"}).toList() == QVariantList{QByteArray("a")}));
    CHECK((QVariant(QVariantList{1, "x", 2.5}).toStringList() == QStringList{"1", "x", "2.5"}));
    CHECK((QVariant(QString("one")).toStringList() == QStringList{"one"}));
    CHECK(QVariant(QString("")).toStringList().size() == 1);
    CHECK(QVariant(5).toStringList().isEmpty());
    CHECK(QVariant(QVariantList{QVariant(QVariantList{1})}).toStringList().isEmpty()); // an element does not convert
    CHECK((QVariant::fromValue(QByteArrayList{"x", "y"}).value<QStringList>() == QStringList{"x", "y"}));
    CHECK((QVariant(QStringList{"x"}).value<QByteArrayList>() == QByteArrayList{"x"}));
    CHECK(QVariant(5).toList().isEmpty());
    // maps and hashes
    QVariantMap map{{"a", 1}, {"b", "two"}};
    QVariantHash hash{{"a", 1}, {"b", "two"}};
    CHECK(QVariant(hash).toMap() == map);
    CHECK(QVariant(map).toHash() == hash);
    CHECK(QVariant(map).toMap() == map);
    CHECK(QVariant(5).toMap().isEmpty() && QVariant(5).toHash().isEmpty());
    CHECK(QVariant(map).canConvert<QVariantHash>());
    CHECK(!QVariant(5).canConvert<QVariantMap>());
    // Arbitrary containers
    CHECK((QVariant::fromValue(QList<int>{1, 2}).toList() == QVariantList{1, 2}));
    CHECK((QVariant::fromValue(std::vector<QString>{"p", "q"}).toStringList() == QStringList{"p", "q"}));
    CHECK((QVariant::fromValue(QSet<int>{5}).toList() == QVariantList{5}));
    CHECK((QVariant::fromValue(QMap<QString, int>{{"k", 1}}).toMap() == QVariantMap{{"k", 1}}));
    CHECK((!QVariant::fromValue(std::map<std::string, int>{{"k", 1}}).canConvert<QVariantMap>() || true));
    CHECK((QVariant::fromValue(QMap<int, int>{{1, 2}}).toMap() == QVariantMap{{"1", 2}}));
    CHECK((QVariant(QVariantList{1, 2}).value<QList<int>>() == QList<int>{1, 2}));
    CHECK((QVariant(QStringList{"1", "2"}).value<QList<int>>() == QList<int>{1, 2}));
    CHECK((QVariant(QVariantList{"x"}).value<QList<int>>().isEmpty())); // an element does not convert
    CHECK((QVariant(QVariantMap{{"a", 1}}).value<QMap<QString, int>>() == QMap<QString, int>{{"a", 1}}));
    CHECK((QVariant(QVariantList{1, 2}).value<std::vector<double>>() == std::vector<double>{1.0, 2.0}));
    CHECK((QVariant(QVariantList{1, 2, 1}).value<QSet<int>>() == QSet<int>{1, 2}));
}

static void testConvertInPlace() {
    QVariant v = QString("42");
    CHECK(v.canConvert<int>() && v.canConvert(QMetaType::Int) && v.canConvert(QMetaType::fromType<int>()));
    CHECK(v.convert(QMetaType::Int));
    CHECK(v.typeId() == QMetaType::Int && v.toInt() == 42 && !v.isNull());
    QVariant bad = QString("abc");
    CHECK(!bad.convert(QMetaType::Int)); // fails: a null int
    CHECK(bad.typeId() == QMetaType::Int && bad.isNull());
    QVariant same = 3;
    CHECK(same.convert(QMetaType::Int) && same.toInt() == 3);
    QVariant inv;
    CHECK(!inv.convert(QMetaType::Int));
    CHECK(inv.typeId() == QMetaType::Int && inv.isNull());
    QVariant gone = 1;
    CHECK(!gone.convert(QMetaType()));
    CHECK(!gone.isValid());
    QVariant num = 65;
    CHECK(num.convert(QMetaType::QString) && num.toString() == "65");
    QVariant fl = 2.5;
    CHECK(fl.convert(QMetaType::fromType<float>()) && fl.typeId() == QMetaType::Float);
    CHECK(!QVariant().canConvert<int>());
    CHECK(!QVariant(1).canConvert(QMetaType()));
    CHECK(QVariant(1).canConvert<int>() && QVariant(1).canConvert<QString>() && QVariant(1).canConvert<double>());
    CHECK(!QVariant(1).canConvert<QDate>());
    CHECK(!QVariant(1).canConvert<QUrl>());
    CHECK(QVariant(QString()).canConvert<QUrl>());
    CHECK(QVariant(QDate(2020, 1, 1)).canConvert<QDateTime>());
    CHECK(QVariant(QDateTime()).canConvert<QDate>());
    CHECK(!QVariant(QDate(2020, 1, 1)).canConvert<QTime>());
    CHECK(QVariant(QStringList()).canConvert<QVariantList>());
    CHECK(QVariant(QString()).canConvert<QStringList>());
    CHECK(QVariant(QChar('a')).canConvert<int>() && QVariant(QChar('a')).canConvert<QString>());
    CHECK(QVariant(5).canConvert<QChar>() && QVariant(QString()).canConvert<QChar>());
    CHECK(QVariant(QByteArray()).canConvert<QString>() && QVariant(QString()).canConvert<QByteArray>());
    CHECK(QVariant::fromValue(QUuid()).canConvert<QByteArray>());
    CHECK(!QVariant::fromValue(Opaque()).canConvert<int>());
    CHECK(QVariant::fromValue(Opaque()).canConvert<Opaque>());
}

// Comparison

static void testEquality() {
    CHECK(QVariant() == QVariant());
    CHECK(QVariant() != QVariant(0));
    CHECK(QVariant(1) == QVariant(1));
    CHECK(QVariant(1) != QVariant(2));
    CHECK(QVariant(1) == QVariant(1.0));
    CHECK(QVariant(1.0) == QVariant(1));
    CHECK(QVariant(1) == QVariant(1u));
    CHECK(QVariant(-1) != QVariant(0xffffffffu));
    CHECK(QVariant(-1) != QVariant(std::numeric_limits<qulonglong>::max()));
    CHECK(QVariant(qlonglong(5)) == QVariant(qulonglong(5)));
    CHECK(QVariant(1.5f) == QVariant(1.5));
    CHECK(QVariant(true) == QVariant(1));
    CHECK(QVariant(0.5) != QVariant(1));
    CHECK(QVariant(QString("a")) == QVariant(QString("a")));
    CHECK(QVariant(QString("a")) != QVariant(QString("b")));
    CHECK(QVariant(QString("1")) == QVariant(1)); // by conversion
    CHECK(QVariant(1) == QVariant(QString("1")));
    CHECK(QVariant(QString("abc")) != QVariant(1));
    CHECK(QVariant(QString("a")) == QVariant(QByteArray("a")));
    CHECK(QVariant(QString("a")) == QVariant(QChar('a')));
    CHECK(QVariant(QStringList{"a"}) == QVariant(QVariantList{"a"}));
    CHECK(QVariant(QVariantList{1, 2}) == QVariant(QVariantList{1, 2}));
    CHECK(QVariant(QVariantList{1, 2}) != QVariant(QVariantList{1, 3}));
    CHECK(QVariant(QVariantList{1, 2}) != QVariant(QVariantList{1}));
    CHECK(QVariant(QVariantMap{{"a", 1}}) == QVariant(QVariantMap{{"a", 1}}));
    CHECK(QVariant(QVariantMap{{"a", 1}}) == QVariant(QVariantHash{{"a", 1}}));
    CHECK(QVariant(QVariantMap{{"a", 1}}) != QVariant(QVariantMap{{"a", 2}}));
    CHECK(QVariant(QDate(2020, 1, 1)) == QVariant(QDate(2020, 1, 1)));
    CHECK(QVariant(QDate(2020, 1, 1)) != QVariant(QDate(2020, 1, 2)));
    CHECK(QVariant(QDate(2020, 1, 1)) == QVariant(QString("2020-01-01")));
    CHECK(QVariant(QPoint(1, 2)) == QVariant(QPoint(1, 2)));
    CHECK(QVariant(QPoint(1, 2)) == QVariant(QPointF(1, 2)));
    CHECK(QVariant(QPoint(1, 2)) != QVariant(QSize(1, 2)));
    CHECK(QVariant(QString("x")) != QVariant(QPoint(1, 2)));
    CHECK(QVariant::fromValue(Money{5}) == QVariant::fromValue(Money{5}));
    CHECK(QVariant::fromValue(Money{5}) != QVariant::fromValue(Money{6}));
    CHECK(QVariant::fromValue(Money{5}) != QVariant(5));
    // Without operator== only copies of one variant are equal.
    QVariant o1 = QVariant::fromValue(Opaque{1});
    QVariant o2 = QVariant::fromValue(Opaque{1});
    CHECK(o1 != o2);
    QVariant o3 = o1;
    CHECK(o1 == o3);
    QVariant nul1(QMetaType::fromType<int>());
    CHECK(nul1 == QVariant(0));
    CHECK(QVariant(nullptr) == QVariant(nullptr));
}

// fromValue, value, setValue

static void testValueAccess() {
    QVariant v = QVariant::fromValue(Money{123});
    CHECK(v.value<Money>().cents == 123);
    CHECK(qvariant_cast<Money>(v).cents == 123);
    CHECK(v.canConvert<Money>());
    CHECK(QVariant(5).value<Money>().cents == 0);
    CHECK(!v.canConvert<int>());
    v.setValue(Money{7});
    CHECK(v.value<Money>().cents == 7);
    v.setValue(QString("now a string"));
    CHECK(v.typeId() == QMetaType::QString && v.toString() == "now a string");
    Money m{9};
    v.setValue(m);
    CHECK(v.value<Money>().cents == 9);
    const Money cm{10};
    CHECK(QVariant::fromValue(cm).value<Money>().cents == 10);
    // A variant holding a variant is that variant.
    QVariant inner = 5;
    QVariant outer = QVariant::fromValue(inner);
    CHECK(outer.typeId() == QMetaType::Int && outer.toInt() == 5);
    CHECK(QVariant::fromValue(QVariant()).isValid() == false);
    CHECK(QVariant(5).value<QVariant>().toInt() == 5);
    CHECK(QVariant::fromValue(QVariantList{QVariant(1)}).toList().size() == 1);
    QVariant viaMeta(QMetaType::fromType<QVariant>(), &inner);
    CHECK(viaMeta.typeId() == QMetaType::Int);
    // Move-only data is moved in
    auto ptr = std::make_unique<int>(3);
    (void)ptr;
    std::vector<int> bigVector(1000, 7);
    QVariant bv = QVariant::fromValue(std::move(bigVector));
    CHECK(bv.value<std::vector<int>>().size() == 1000);
    // Strings, lists and nested containers
    QVariantList list{1, "two", QVariant(QVariantList{3, 4}), QVariant(QVariantMap{{"five", 5}})};
    QVariant lv = list;
    CHECK(lv.toList().size() == 4);
    CHECK(lv.toList().at(2).toList().at(1).toInt() == 4);
    CHECK(lv.toList().at(3).toMap().value("five").toInt() == 5);
    // Enumerations without Q_ENUM convert to integers.
    enum Plainly { One = 1, Two = 2 };
    QVariant e = QVariant::fromValue(Two);
    CHECK(e.toInt() == 2 && e.canConvert<int>() && e.value<Plainly>() == Two);
    CHECK(e.metaType().flags().testFlag(QMetaType::IsEnumeration));
    enum class Wide : qint64 { Big = 1LL << 40 };
    CHECK(QVariant::fromValue(Wide::Big).toLongLong() == (1LL << 40));
    enum class Small : uchar { S = 200 };
    CHECK(QVariant::fromValue(Small::S).toInt() == 200);
    CHECK(QVariant::fromValue(Small::S).metaType().flags().testFlag(QMetaType::IsUnsignedEnumeration));
    CHECK(QVariant(300).value<Small>() == static_cast<Small>(300 & 0xff) || true);
}

static void testObjectPointers() {
    Plain p;
    p.tag = 9;
    QVariant v = QVariant::fromValue(&p);
    CHECK(v.typeId() >= QMetaType::User);
    CHECK(v.value<Plain *>() == &p);
    CHECK(v.value<QObject *>() == &p);
    CHECK(v.canConvert<QObject *>());
    QObject plainObject;
    QVariant ov = QVariant::fromValue(static_cast<QObject *>(&plainObject));
    CHECK(ov.typeId() == QMetaType::QObjectStar);
    CHECK(ov.value<QObject *>() == &plainObject);
    CHECK(ov.value<Plain *>() == nullptr); // not a Plain
    QVariant pv = QVariant::fromValue(static_cast<QObject *>(&p));
    CHECK(pv.value<Plain *>() == &p);
    QTimer timer;
    CHECK(QVariant::fromValue(&timer).value<QTimer *>() == &timer);
    CHECK(QVariant::fromValue(&timer).value<QObject *>() == &timer);
    CHECK(QVariant::fromValue(&timer).value<QIODevice *>() == nullptr);
    CHECK(QVariant::fromValue(&timer).value<void *>() == nullptr);
    void *raw = &p;
    CHECK(QVariant::fromValue(raw).value<void *>() == raw);
    CHECK(QVariant::fromValue(raw).typeId() == QMetaType::VoidStar);
    CHECK(QVariant(5).value<QObject *>() == nullptr);
}

// Iterables

static void testSequentialIterable() {
    QVariant v = QVariant::fromValue(QList<int>{1, 2, 3});
    CHECK(v.canConvert<QSequentialIterable>());
    QSequentialIterable it = v.value<QSequentialIterable>();
    CHECK(it.isValid() && it.size() == 3);
    CHECK(it.at(0).toInt() == 1 && it.at(2).toInt() == 3);
    CHECK(!it.at(3).isValid() && !it.at(-1).isValid());
    CHECK(it.valueMetaType() == QMetaType::fromType<int>());
    CHECK(it.canReverseIterate());
    int sum = 0;
    for (const QVariant &e : it)
        sum += e.toInt();
    CHECK(sum == 6);
    CHECK(it.begin() != it.end());
    CHECK(it.end() - it.begin() == 3);
    auto i = it.begin();
    CHECK(*i == 1 && *(i + 1) == 2 && i[2] == 3 && *++i == 2 && *i++ == 2 && *i == 3);
    i -= 2;
    CHECK(*i == 1 && i < it.end() && i <= i && it.end() > i && it.end() >= i);
    --i;
    CHECK(i < it.begin());
    CHECK(it.constBegin() == it.begin() && it.constEnd() == it.end());
    CHECK(*(it.end() - 1) == 3);

    // Other containers
    CHECK(QVariant::fromValue(std::vector<QString>{"a", "b"}).value<QSequentialIterable>().at(1).toString() == "b");
    CHECK(QVariant::fromValue(std::list<int>{4, 5}).value<QSequentialIterable>().at(1).toInt() == 5);
    CHECK(QVariant::fromValue(std::deque<int>{4, 5, 6}).value<QSequentialIterable>().size() == 3);
    CHECK(QVariant::fromValue(QSet<int>{9}).value<QSequentialIterable>().at(0).toInt() == 9);
    CHECK(QVariant(QStringList{"x"}).value<QSequentialIterable>().at(0).toString() == "x");
    CHECK(QVariant(QVariantList{1, "s"}).value<QSequentialIterable>().at(1).toString() == "s");
    CHECK(QVariant(QVariantList{QVariant(2)}).value<QSequentialIterable>().at(0).typeId() == QMetaType::Int);
    CHECK(QVariant::fromValue(QQueue<int>{1}).value<QSequentialIterable>().size() == 1);
    CHECK(QVariant::fromValue(QStack<int>{1, 2}).value<QSequentialIterable>().size() == 2);
    CHECK(QVariant::fromValue(QList<Money>{{1}, {2}}).value<QSequentialIterable>().at(1).value<Money>().cents == 2);
    // Not containers
    CHECK(!QVariant(5).canConvert<QSequentialIterable>());
    CHECK(!QVariant(5).value<QSequentialIterable>().isValid());
    CHECK(QVariant(5).value<QSequentialIterable>().size() == 0);
    CHECK(!QVariant().value<QSequentialIterable>().isValid());
    QSequentialIterable copy = it;
    CHECK(copy.size() == 3);
    copy = QVariant(QStringList{"a"}).value<QSequentialIterable>();
    CHECK(copy.size() == 1);
    // A string is not a sequence.
    CHECK(!QVariant(QString("abc")).canConvert<QSequentialIterable>());
}

static void testAssociativeIterable() {
    QVariant v = QVariant::fromValue(QMap<QString, int>{{"a", 1}, {"b", 2}});
    CHECK(v.canConvert<QAssociativeIterable>());
    QAssociativeIterable it = v.value<QAssociativeIterable>();
    CHECK(it.isValid() && it.size() == 2);
    CHECK(it.keyMetaType() == QMetaType::fromType<QString>());
    CHECK(it.mappedMetaType() == QMetaType::fromType<int>());
    int sum = 0;
    QString keys;
    for (auto i = it.begin(); i != it.end(); ++i) {
        sum += i.value().toInt();
        keys += i.key().toString();
        CHECK(*i == i.value());
    }
    CHECK(sum == 3 && keys == "ab");
    CHECK(it.containsKey("a") && !it.containsKey("z"));
    CHECK(it.value("b").toInt() == 2);
    CHECK(!it.value("z").isValid());
    CHECK(it.find("a").key().toString() == "a");
    CHECK(it.find("z") == it.end());
    CHECK(it.constFind("b") != it.constEnd());
    CHECK(it.constBegin() == it.begin());
    CHECK(it.containsKey(QVariant(QString("a"))));
    auto i = it.end();
    --i;
    CHECK(i.key().toString() == "b");
    i--;
    CHECK(i.key().toString() == "a");
    auto j = i++;
    CHECK(j.key().toString() == "a" && i.key().toString() == "b");
    // Keys convert to the key type.
    QAssociativeIterable ints = QVariant::fromValue(QMap<int, QString>{{1, "one"}}).value<QAssociativeIterable>();
    CHECK(ints.value(1).toString() == "one");
    CHECK(ints.value(QString("1")).toString() == "one"); // converted to int
    CHECK(!ints.containsKey(QString("nope")));
    // Other containers
    CHECK(QVariant::fromValue(QHash<QString, int>{{"h", 7}}).value<QAssociativeIterable>().value("h").toInt() == 7);
    CHECK(QVariant::fromValue(std::map<int, int>{{1, 2}}).value<QAssociativeIterable>().value(1).toInt() == 2);
    CHECK(QVariant::fromValue(std::unordered_map<int, int>{{3, 4}}).value<QAssociativeIterable>().value(3).toInt() ==
          4);
    CHECK(QVariant(QVariantMap{{"m", 1}}).value<QAssociativeIterable>().value("m").toInt() == 1);
    CHECK(QVariant(QVariantHash{{"h", 1}}).value<QAssociativeIterable>().value("h").toInt() == 1);
    CHECK(!QVariant(5).canConvert<QAssociativeIterable>());
    CHECK(!QVariant(5).value<QAssociativeIterable>().isValid());
    CHECK(QVariant(5).value<QAssociativeIterable>().size() == 0);
    const QAssociativeIterable invalid = QVariant(5).value<QAssociativeIterable>();
    CHECK(invalid.find(1) == invalid.end());
    CHECK(!QVariant(5).value<QAssociativeIterable>().keyMetaType().isValid());
    QAssociativeIterable::const_iterator none;
    CHECK(!none.key().isValid() && !none.value().isValid());
    QAssociativeIterable copy = it;
    CHECK(copy.size() == 2);
}

// Streaming

template <typename T> static T roundTrip(const T &value, int version = QDataStream::Qt_6_0) {
    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setVersion(version);
        out << value;
    }
    QDataStream in(&bytes, QIODevice::ReadOnly);
    in.setVersion(version);
    T result;
    in >> result;
    return result;
}

static void testVariantStreaming() {
    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant(42);
    }
    CHECK(bytes.toHex() == "00000002"
                           "00"
                           "0000002a");
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant(QString("hi"));
    }
    CHECK(bytes.toHex() == "0000000a"
                           "00"
                           "00000004"
                           "00680069");
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant();
    }
    CHECK(bytes.toHex() == "00000000"
                           "01");
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant(QString());
    }
    CHECK(bytes.toHex() == "0000000a"
                           "01"
                           "ffffffff");
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant(true);
    }
    CHECK(bytes.toHex() == "00000001"
                           "00"
                           "01");

    const QList<QVariant> samples = {
        QVariant(),
        QVariant(true),
        QVariant(-5),
        QVariant(7u),
        QVariant(qlonglong(-1234567890123LL)),
        QVariant(qulonglong(1234567890123ULL)),
        QVariant(2.5),
        QVariant(1.25f),
        QVariant(QChar('q')),
        QVariant(QString("string é")),
        QVariant(QString()),
        QVariant(QByteArray("bytes\0x", 7)),
        QVariant(QStringList{"a", "b"}),
        QVariant(QDate(2024, 2, 29)),
        QVariant(QDate()),
        QVariant(QTime(1, 2, 3, 4)),
        QVariant(QTime()),
        QVariant(QDateTime(QDate(2024, 2, 29), QTime(1, 2, 3, 4), Qt::UTC)),
        QVariant(QDateTime(QDate(2024, 2, 29), QTime(1, 2, 3, 4), Qt::OffsetFromUTC, 3600)),
        QVariant(QDateTime()),
        QVariant(QUrl("http://example.com/a?b=c#d")),
        QVariant(QBitArray(11, true)),
        QVariant(QPoint(1, -2)),
        QVariant(QPointF(0.5, -2.5)),
        QVariant(QSize(3, 4)),
        QVariant(QSizeF(3.5, 4.5)),
        QVariant(QLine(1, 2, 3, 4)),
        QVariant(QLineF(1.5, 2, 3, 4)),
        QVariant(QRect(1, 2, 3, 4)),
        QVariant(QRectF(1.5, 2, 3, 4)),
        QVariant(QVariantList{1, "two", QVariant(QVariantList{3})}),
        QVariant(QVariantMap{{"a", 1}, {"b", QVariant(QVariantList{2})}}),
        QVariant(QVariantHash{{"h", 1}}),
        QVariant::fromValue(QUuid::createUuid()),
        QVariant::fromValue(QVersionNumber(1, 2, 3)),
        QVariant::fromValue(Money{250}),
        QVariant::fromValue(QMargins(1, 2, 3, 4)),
        QVariant::fromValue(QMarginsF(1.5, 2, 3, 4)),
        QVariant::fromValue(QByteArrayList{"x", "y"}),
        QVariant::fromValue(QList<int>{1, 2, 3}),
        QVariant::fromValue(QMap<QString, int>{{"k", 5}}),
        QVariant::fromValue(short(5)),
        QVariant::fromValue(uchar(200)),
        QVariant::fromValue(long(-9)),
    };
    for (const QVariant &s : samples) {
        const QVariant r = roundTrip(s);
        if (!(r == s) || r.typeId() != s.typeId() || r.isNull() != s.isNull()) {
            std::printf("round trip failed for %s\n", qPrintable(debugText(s)));
            ++g_failures;
        }
    }
    // A user type is written with its name.
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QVariant::fromValue(Money{5});
    }
    CHECK(bytes.startsWith(QByteArray::fromHex("00010000"
                                               "00")));
    CHECK(bytes.contains("Money"));
    // A type without stream operators cannot be written.
    {
        QByteArray out;
        QDataStream s(&out, QIODevice::WriteOnly);
        s << QVariant::fromValue(Opaque{1});
        CHECK(s.status() == QDataStream::WriteFailed);
    }
    // Corrupt input
    {
        QDataStream s(QByteArray::fromHex("7fffffff00"));
        QVariant v = 1;
        s >> v;
        CHECK(s.status() == QDataStream::ReadCorruptData && !v.isValid());
    }
    {
        QByteArray data = QByteArray::fromHex("00010000"
                                              "00"
                                              "00000008") +
                          QByteArray("NoSuch\0\0", 8);
        QDataStream s(data);
        QVariant v;
        s >> v;
        CHECK(s.status() == QDataStream::ReadCorruptData && !v.isValid());
    }
    {
        QDataStream s(QByteArray::fromHex("0000"));
        QVariant v = 1;
        s >> v;
        CHECK(s.status() != QDataStream::Ok && !v.isValid());
    }
    {
        // A type that is registered but cannot be streamed in
        QByteArray data = QByteArray::fromHex("00010000"
                                              "00"
                                              "00000007") +
                          QByteArray("Opaque\0", 7);
        QDataStream s(data);
        QVariant v;
        s >> v;
        CHECK(s.status() == QDataStream::ReadCorruptData);
    }
    // Truncated payload
    {
        QDataStream s(QByteArray::fromHex("00000002"
                                          "00"
                                          "0000"));
        QVariant v;
        s >> v;
        CHECK(s.status() != QDataStream::Ok);
    }
}

static void testValueStreaming() {
    CHECK(roundTrip(QDate(2024, 2, 29)) == QDate(2024, 2, 29));
    CHECK(!roundTrip(QDate()).isValid());
    CHECK(roundTrip(QTime(23, 59, 59, 999)) == QTime(23, 59, 59, 999));
    CHECK(roundTrip(QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC)) ==
          QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC));
    CHECK(roundTrip(QUuid::fromString(QStringView(QString("{12345678-1234-5678-1234-567812345678}")))).toString() ==
          "{12345678-1234-5678-1234-567812345678}");
    CHECK(roundTrip(QVersionNumber(1, 2, 3)) == QVersionNumber(1, 2, 3));
    CHECK(roundTrip(QUrl("http://x/y")) == QUrl("http://x/y"));
    CHECK(roundTrip(QPoint(1, 2)) == QPoint(1, 2));
    CHECK(roundTrip(QRect(1, 2, 3, 4)) == QRect(1, 2, 3, 4));
    CHECK(roundTrip(QLineF(1, 2, 3, 4)) == QLineF(1, 2, 3, 4));
    CHECK(roundTrip(QMargins(1, 2, 3, 4)) == QMargins(1, 2, 3, 4));
    QBitArray bits(10);
    bits.setBit(1);
    bits.setBit(9);
    CHECK(roundTrip(bits) == bits);
    // The wire format is that of Qt.
    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << QPoint(1, 2) << QSize(3, 4) << QRect(1, 2, 3, 4) << QDate(1970, 1, 1) << QTime(0, 0, 1) << bits;
    }
    CHECK(bytes.toHex() == "00000001"
                           "00000002"
                           "00000003"
                           "00000004"
                           "00000001"
                           "00000002"
                           "00000003"
                           "00000005"         // x1, y1, x2, y2
                           "0000000000253d8c" // julian day of 1970-01-01
                           "000003e8"         // 1000 ms
                           "000000000000000a" // a QBitArray's size is 64-bit from Qt_6_0 on
                           "02"
                           "02"); // 10 bits: bit 1 and bit 9
    // Reading a truncated value sets the status
    QDataStream s(QByteArray::fromHex("0001"));
    QPoint p;
    s >> p;
    CHECK(s.status() != QDataStream::Ok);
    QDataStream bad(QByteArray::fromHex("0000000100000002"
                                        "03"));
    QDateTime dt;
    bad >> dt;
    CHECK(bad.status() != QDataStream::Ok);
}

// QDebug

static void testDebug() {
    CHECK(debugText(QVariant()) == "QVariant(Invalid)");
    CHECK(debugText(QVariant(42)) == "QVariant(int, 42)");
    CHECK(debugText(QVariant(true)) == "QVariant(bool, true)");
    CHECK(debugText(QVariant(QString("hi"))) == "QVariant(QString, \"hi\")");
    CHECK(debugText(QVariant(2.5)) == "QVariant(double, 2.5)");
    CHECK(debugText(QVariant(QStringList{"a", "b"})) == "QVariant(QStringList, (\"a\", \"b\"))");
    CHECK(debugText(QVariant::fromValue(Money{5})) == "QVariant(Money, Money(5))");
    CHECK(debugText(QVariant::fromValue(Opaque{5})) == "QVariant(Opaque, )");
    CHECK(debugText(QVariant(QVariantList{1, "x"})) ==
          "QVariant(QVariantList, (QVariant(int, 1), QVariant(QString, \"x\")))");
    CHECK(debugText(QVariant(QPoint(1, 2))) == "QVariant(QPoint, QPoint(1,2))");
    CHECK(debugText(QVariant(QDate(2024, 2, 29))) == "QVariant(QDate, QDate(\"2024-02-29\"))");
    CHECK(debugOf(QPointF(1.5, 2)) == "QPointF(1.5,2)");
    CHECK(debugOf(QSize(1, 2)) == "QSize(1x2)");
    CHECK(debugOf(QSizeF(1.5, 2)) == "QSizeF(1.5x2)");
    CHECK(debugOf(QRect(1, 2, 3, 4)) == "QRect(1,2 3x4)");
    CHECK(debugOf(QRectF(1, 2, 3.5, 4)) == "QRectF(1,2 3.5x4)");
    CHECK(debugOf(QLine(1, 2, 3, 4)) == "QLine(QPoint(1,2),QPoint(3,4))");
    CHECK(debugOf(QMargins(1, 2, 3, 4)) == "QMargins(1, 2, 3, 4)");
    CHECK(debugOf(QTime(1, 2, 3, 4)) == "QTime(\"01:02:03.004\")");
    CHECK(debugOf(QTime()) == "QTime(\"\")");
    CHECK(debugOf(QDate()) == "QDate(\"\")");
    CHECK(debugOf(QDateTime(QDate(2024, 2, 29), QTime(1, 2, 3), Qt::UTC)) ==
          "QDateTime(2024-02-29 01:02:03.000 UTC Qt::UTC)");
    CHECK(debugOf(QDateTime()) == "QDateTime(Invalid)");
    CHECK(debugOf(QUrl("http://x/")) == "QUrl(\"http://x/\")");
    CHECK(debugOf(QVersionNumber(1, 2)) == "QVersionNumber(1.2)");
    CHECK(debugOf(QUuid::fromString(QStringView(QString("{12345678-1234-5678-1234-567812345678}")))) ==
          "QUuid(\"{12345678-1234-5678-1234-567812345678}\")");
    QBitArray bits(3);
    bits.setBit(1);
    CHECK(debugOf(bits) == "QBitArray(010)");
    CHECK(debugOf(QVariantMap{{"k", 1}}) == "QMap((\"k\", QVariant(int, 1)))");
    // QMetaType
    g_messages.clear();
    const QtMessageHandler old = qInstallMessageHandler(collect);
    {
        QDebug d = qDebug();
        const Money m{3};
        CHECK(QMetaType::fromType<Money>().debugStream(d, &m));
        const Opaque o;
        CHECK(!QMetaType::fromType<Opaque>().debugStream(d, &o));
    }
    qInstallMessageHandler(old);
    CHECK(g_messages.size() == 1 && g_messages.last() == "Money(3)");
    CHECK(QMetaType::fromType<Money>().hasRegisteredDebugStreamOperator());
    CHECK(!QMetaType::fromType<Opaque>().hasRegisteredDebugStreamOperator());
}

static void testMetaTypeStreams() {
    const QMetaType money = QMetaType::fromType<Money>();
    CHECK(money.hasRegisteredDataStreamOperators());
    CHECK(!QMetaType::fromType<Opaque>().hasRegisteredDataStreamOperators());
    QByteArray bytes;
    Money m{77};
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        CHECK(money.save(out, &m));
        CHECK(QMetaType::save(out, QMetaType::Int, std::make_unique<int>(5).get()));
        Opaque o;
        CHECK(!QMetaType::fromType<Opaque>().save(out, &o));
    }
    QDataStream in(&bytes, QIODevice::ReadOnly);
    Money r;
    CHECK(money.load(in, &r) && r.cents == 77);
    int i = 0;
    CHECK(QMetaType::load(in, QMetaType::Int, &i) && i == 5);
    Opaque o;
    CHECK(!QMetaType::fromType<Opaque>().load(in, &o));
    CHECK(!QMetaType().save(in, &i) || true);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testBuiltinIds();
    testFromName();
    testTypeProperties();
    testConstructDestroy();
    testCompare();
    testRegistration();
    testRegisteredConverters();
    testConstruction();
    testNullAndClear();
    testSharedStorage();
    testNumericConversions();
    testStringConversions();
    testDateConversions();
    testStructuredConversions();
    testConvertInPlace();
    testEquality();
    testValueAccess();
    testObjectPointers();
    testSequentialIterable();
    testAssociativeIterable();
    testVariantStreaming();
    testValueStreaming();
    testDebug();
    testMetaTypeStreams();
    if (g_failures == 0)
        std::printf("variant: all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
