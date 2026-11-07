// MIT License - see LICENSE
#include <QBasicTimer>
#include <QBitArray>
#include <QByteArrayView>
#include <QCache>
#include <QContiguousCache>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QLibraryInfo>
#include <QLine>
#include <QList>
#include <QListIterator>
#include <QMap>
#include <QMapIterator>
#include <QMargins>
#include <QMessageAuthenticationCode>
#include <QMutableListIterator>
#include <QMutableMapIterator>
#include <QPoint>
#include <QRandomGenerator>
#include <QRect>
#include <QSize>
#include <QSpan>
#include <QStringView>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVarLengthArray>
#include <QVersionNumber>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

struct HashVector {
    QCryptographicHash::Algorithm algorithm;
    const char *input;
    const char *digest;
};
struct DateVector {
    int y, m, d;
    qint64 daysFromEpoch;
    int dow, doy, week, weekYear;
};
#include "test_core_values_vectors.inc"

static void testHashes() {
    for (const HashVector &v : kHashVectors) {
        const QByteArray in = QByteArray::fromHex(QByteArray(v.input));
        CHECK(QCryptographicHash::hash(in, v.algorithm).toHex() == QByteArray(v.digest));
        // The same data split across calls, with result() in between.
        QCryptographicHash h(v.algorithm);
        const qsizetype half = in.size() / 3;
        h.addData(in.left(half));
        (void)h.result();
        h.addData(in.mid(half));
        CHECK(h.result().toHex() == QByteArray(v.digest));
        h.reset();
        h.addData(in);
        CHECK(h.result().toHex() == QByteArray(v.digest));
    }
    for (const HashVector &v : kHmacVectors) {
        const QByteArray key = QByteArray::fromHex(QByteArray(v.input));
        CHECK(QMessageAuthenticationCode::hash(QByteArray("The quick brown fox"), key, v.algorithm).toHex() == QByteArray(v.digest));
    }
    // RFC 1320 test suite.
    CHECK(QCryptographicHash::hash(QByteArray("abc"), QCryptographicHash::Md4).toHex() == "a448017aaf21d8525fc10ae87aa6729d");
    CHECK(QCryptographicHash::hash(QByteArray(""), QCryptographicHash::Md4).toHex() == "31d6cfe0d16ae931b73c59d7e0c089c0");
    // Keccak differs from SHA-3 only in padding; this is the published empty-input digest.
    CHECK(QCryptographicHash::hash(QByteArray(""), QCryptographicHash::Keccak_256).toHex() == "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
    CHECK(QCryptographicHash::hashLength(QCryptographicHash::Sha3_512) == 64);
}

static void testDates() {
    const QDate epoch(1970, 1, 1);
    for (const DateVector &v : kDateVectors) {
        const QDate d(v.y, v.m, v.d);
        CHECK(d.isValid());
        CHECK(epoch.daysTo(d) == v.daysFromEpoch);
        CHECK(d.dayOfWeek() == v.dow);
        CHECK(d.dayOfYear() == v.doy);
        int weekYear = 0;
        CHECK(d.weekNumber(&weekYear) == v.week);
        CHECK(weekYear == v.weekYear);
        CHECK(d.year() == v.y && d.month() == v.m && d.day() == v.d);
        CHECK(epoch.addDays(v.daysFromEpoch) == d);
    }
    CHECK(!QDate(2023, 2, 29).isValid());
    CHECK(!QDate(0, 1, 1).isValid());
    CHECK(QDate(-1, 1, 1).isValid());
    CHECK(QDate(1, 1, 1).addDays(-1) == QDate(-1, 12, 31));
    CHECK(QDate(2024, 1, 31).addMonths(1) == QDate(2024, 2, 29));
    CHECK(QDate(2024, 2, 29).addYears(1) == QDate(2025, 2, 28));
    CHECK(QDate(2024, 3, 15).addMonths(-3) == QDate(2023, 12, 15));
    CHECK(QDate(1995, 5, 20).toString(Qt::ISODate) == "1995-05-20");
    CHECK(QDate(1995, 5, 20).toString(Qt::TextDate) == "Sat May 20 1995");
    CHECK(QDate(1995, 5, 20).toString(QString("dddd, d MMMM yy")) == "Saturday, 20 May 95");
    CHECK(QDate::fromString(QString("1995-05-20"), Qt::ISODate) == QDate(1995, 5, 20));
    CHECK(QDate::fromString(QString("20 May 1995"), QString("d MMM yyyy")) == QDate(1995, 5, 20));
    CHECK(!QDate::fromString(QString("1995-13-20"), Qt::ISODate).isValid());

    CHECK(QTime(13, 5, 9, 7).toString(QString("hh:mm:ss.zzz")) == "13:05:09.007");
    CHECK(QTime(13, 5).toString(QString("h:mm AP")) == "1:05 PM");
    CHECK(QTime(0, 5).toString(QString("h:mm ap")) == "12:05 am");
    CHECK(QTime(23, 59, 59).addSecs(2) == QTime(0, 0, 1));
    CHECK(QTime(0, 0, 1).addSecs(-2) == QTime(23, 59, 59));
    CHECK(!QTime(24, 0).isValid());
    CHECK(QTime().isNull() && !QTime(25, 0).isNull());
    CHECK(QTime::fromString(QString("12:34:56.5"), Qt::ISODate) == QTime(12, 34, 56, 500));
    CHECK(QTime::fromString(QString("1:30 PM"), QString("h:mm AP")) == QTime(13, 30));

    const QDateTime u = QDateTime::fromSecsSinceEpoch(1000000000, Qt::UTC);
    CHECK(u.toString(Qt::ISODate) == "2001-09-09T01:46:40Z");
    CHECK(u.toSecsSinceEpoch() == 1000000000);
    CHECK(u.toString(Qt::RFC2822Date) == "09 Sep 2001 01:46:40 +0000");
    const QDateTime o = u.toOffsetFromUtc(5400);
    CHECK(o == u);
    CHECK(o.toString(Qt::ISODate) == "2001-09-09T03:16:40+01:30");
    CHECK(QDateTime::fromString(QString("2001-09-09T03:16:40+01:30"), Qt::ISODate) == u);
    CHECK(QDateTime::fromString(QString("2001-09-09T01:46:40Z"), Qt::ISODate) == u);
    CHECK(QDateTime::fromString(QString("Sun, 09 Sep 2001 03:16:40 +0130"), Qt::RFC2822Date) == u);
    CHECK(QDateTime::fromMSecsSinceEpoch(-1, Qt::UTC).toString(Qt::ISODateWithMs) == "1969-12-31T23:59:59.999Z");
    CHECK(u.addSecs(86400).date() == QDate(2001, 9, 10));
    CHECK(u.toLocalTime() == u);
    CHECK(u.toLocalTime().toUTC().toString(Qt::ISODate) == "2001-09-09T01:46:40Z");
    CHECK(u.secsTo(u.addSecs(90)) == 90);
    CHECK(QDateTime::currentDateTime().isValid());
    const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    CHECK(qAbs(nowMs - QDateTime::currentMSecsSinceEpoch()) < 5000);
}

static void testGeometry() {
    QRect r(10, 20, 30, 40);
    CHECK(r.right() == 39 && r.bottom() == 59 && r.width() == 30);
    CHECK(QRect().isNull() && QRect().isEmpty() && !QRect().isValid());
    CHECK(r.contains(QPoint(39, 59)) && !r.contains(QPoint(40, 59)));
    CHECK(!r.contains(QPoint(10, 20), true));
    CHECK(r.intersected(QRect(30, 30, 100, 100)) == QRect(30, 30, 10, 30));
    CHECK(!r.intersects(QRect(40, 20, 5, 5)));
    CHECK((r | QRect(0, 0, 5, 5)) == QRect(0, 0, 40, 60));
    CHECK(r.translated(1, 2).topLeft() == QPoint(11, 22));
    CHECK(r.adjusted(1, 1, -1, -1).size() == QSize(28, 38));
    CHECK(r.marginsAdded(QMargins(1, 2, 3, 4)) == QRect(9, 18, 34, 46));
    QRect moved = r;
    moved.moveCenter(QPoint(100, 100));
    CHECK(moved.center() == QPoint(100, 100) && moved.size() == r.size());
    CHECK(QRect(QPoint(5, 5), QPoint(1, 1)).normalized() == QRect(1, 1, 5, 5));

    QRectF f(1.5, 2.5, 10, 20);
    CHECK(f.right() == 11.5 && f.center() == QPointF(6.5, 12.5));
    CHECK(f.intersected(QRectF(0, 0, 5, 5)) == QRectF(1.5, 2.5, 3.5, 2.5));
    CHECK(f.toAlignedRect() == QRect(1, 2, 11, 21));
    CHECK(QRectF(QRect(1, 2, 3, 4)).width() == 3);

    CHECK(QPoint(3, -4).manhattanLength() == 7);
    CHECK(QPoint(1, 2) + QPoint(3, 4) == QPoint(4, 6));
    CHECK(QPoint(3, 4) * 1.5 == QPoint(5, 6));
    CHECK(QPointF(0.1 + 0.2, 1) == QPointF(0.3, 1));
    CHECK(QPoint::dotProduct(QPoint(1, 2), QPoint(3, 4)) == 11);

    CHECK(QSize(10, 20).transposed() == QSize(20, 10));
    CHECK(!QSize().isValid() && QSize().isEmpty());
    CHECK(QSize(100, 50).scaled(40, 40, Qt::KeepAspectRatio) == QSize(40, 20));
    CHECK(QSize(100, 50).scaled(40, 40, Qt::KeepAspectRatioByExpanding) == QSize(80, 40));
    CHECK(QSize(100, 50).scaled(40, 40, Qt::IgnoreAspectRatio) == QSize(40, 40));
    CHECK(QSize(1, 9).expandedTo(QSize(5, 2)) == QSize(5, 9));

    CHECK(QLine(1, 2, 4, 6).dx() == 3);
    QLineF l(0, 0, 3, 4);
    CHECK(qFuzzyCompare(l.length(), 5.0));
    CHECK(qFuzzyCompare(QLineF(0, 0, 0, -1).angle(), 90.0));
    CHECK(qFuzzyCompare(QLineF(0, 0, -1, 0).angle(), 180.0));
    QPointF hit;
    CHECK(QLineF(0, 0, 2, 2).intersects(QLineF(0, 2, 2, 0), &hit) == QLineF::BoundedIntersection);
    CHECK(hit == QPointF(1, 1));
    CHECK(QLineF(0, 0, 1, 0).intersects(QLineF(0, 1, 1, 1)) == QLineF::NoIntersection);
    CHECK(QLineF(0, 0, 1, 1).intersects(QLineF(5, 0, 4, 1)) == QLineF::UnboundedIntersection);
}

// Counts live instances, to catch lifetime errors in the manual storage.
struct Tracked {
    static int live;
    std::string s;
    Tracked() : s("x") { ++live; }
    Tracked(const char *v) : s(v) { ++live; }
    Tracked(const Tracked &o) : s(o.s) { ++live; }
    Tracked(Tracked &&o) noexcept : s(std::move(o.s)) { ++live; }
    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) = default;
    ~Tracked() { --live; }
    bool operator==(const Tracked &o) const { return s == o.s; }
};
int Tracked::live = 0;

static void testContainers() {
    {
        QVarLengthArray<Tracked, 2> a;
        a.append("one");
        a.append("two");
        CHECK(a.capacity() == 2);
        a.append("three"); // spills to the heap
        CHECK(a.size() == 3 && a.capacity() >= 3);
        a.append(a[0]); // an element of the array itself, during growth
        a.append(a[0]);
        CHECK(a.last().s == "one");
        a.prepend("zero");
        CHECK(a[0].s == "zero" && a[1].s == "one");
        a.remove(1, 2);
        CHECK(a[1].s == "three");
        QVarLengthArray<Tracked, 2> b = a;
        CHECK(a == b);
        QVarLengthArray<Tracked, 2> c = std::move(b);
        CHECK(c == a && b.isEmpty());
        c.resize(1);
        CHECK(c.size() == 1);
        QVarLengthArray<Tracked, 2> small{"p"};
        QVarLengthArray<Tracked, 2> moved = std::move(small); // inline storage path
        CHECK(moved.size() == 1 && moved[0].s == "p");
        a = moved;
        CHECK(a == moved);
        a.squeeze();
    }
    CHECK(Tracked::live == 0);

    QBitArray bits(8);
    CHECK(bits.count(true) == 0);
    bits.setBit(1);
    bits[3] = true;
    CHECK(bits.testBit(1) && bits[3] && bits.count(true) == 2);
    CHECK(bits.toggleBit(1) && !bits.testBit(1));
    QBitArray other(4, true);
    CHECK((bits | other).count(true) == 4);
    CHECK((bits & other).count(true) == 1);
    CHECK((bits ^ other).count(true) == 3);
    CHECK((~bits).count(true) == 7);
    CHECK(QBitArray().isNull() && !QBitArray(0).isNull());

    {
        QCache<int, Tracked> cache(3);
        CHECK(cache.insert(1, new Tracked("a")));
        CHECK(cache.insert(2, new Tracked("b")));
        CHECK(cache.insert(3, new Tracked("c")));
        CHECK(cache.object(1)); // 1 is now the most recent
        CHECK(cache.insert(4, new Tracked("d")));
        CHECK(!cache.contains(2) && cache.contains(1));
        CHECK(!cache.insert(5, new Tracked("too big"), 4));
        CHECK(cache.totalCost() == 3);
        CHECK(cache.insert(1, new Tracked("replaced"), 2));
        CHECK(cache.object(1)->s == "replaced" && cache.totalCost() <= 3);
        std::unique_ptr<Tracked> taken(cache.take(1));
        CHECK(taken && !cache.contains(1));
        cache.setMaxCost(0);
        CHECK(cache.isEmpty() && cache.totalCost() == 0);
    }
    CHECK(Tracked::live == 0);

    QContiguousCache<int> window(3);
    for (int i = 0; i < 5; ++i)
        window.append(i);
    CHECK(window.firstIndex() == 2 && window.lastIndex() == 4 && window.at(3) == 3);
    window.prepend(1);
    CHECK(window.firstIndex() == 1 && window.lastIndex() == 3);
    window.insert(10, 99);
    CHECK(window.size() == 1 && window.firstIndex() == 10 && window.at(10) == 99);
    window.insert(11, 100);
    CHECK(window.size() == 2 && window.takeLast() == 100);

    int raw[] = {1, 2, 3, 4};
    QSpan<int> span(raw);
    CHECK(span.size() == 4 && span.first(2).back() == 2 && span.last(1).front() == 4 && span.sliced(1, 2).size() == 2);
    QList<int> list{5, 6};
    QSpan<int> fromList(list);
    fromList[0] = 9;
    CHECK(list[0] == 9);
    QSpan<const int> constant = span;
    CHECK(constant.size_bytes() == 16);

    QList<int> values{1, 2, 3, 4};
    QListIterator<int> li(values);
    int sum = 0;
    while (li.hasNext())
        sum += li.next();
    CHECK(sum == 10 && !li.hasNext());
    CHECK(li.previous() == 4 && li.peekPrevious() == 3);
    li.toFront();
    CHECK(li.findNext(3) && li.peekNext() == 4);
    QMutableListIterator<int> mi(values);
    while (mi.hasNext())
        if (mi.next() % 2 == 0)
            mi.remove();
    CHECK(values == QList<int>({1, 3}));
    mi.toFront();
    mi.next();
    mi.setValue(7);
    mi.insert(8);
    CHECK(values == QList<int>({7, 8, 3}));
    mi.toBack();
    CHECK(mi.previous() == 3);
    mi.remove();
    CHECK(values == QList<int>({7, 8}));

    QMap<int, QString> map;
    map.insert(1, "a");
    map.insert(2, "b");
    map.insert(3, "c");
    QMapIterator<int, QString> ci(map);
    ci.toBack();
    CHECK(ci.hasPrevious());
    ci.previous();
    CHECK(ci.key() == 3 && ci.value() == "c");
    QMutableMapIterator<int, QString> mm(map);
    while (mm.hasNext()) {
        mm.next();
        if (mm.key() == 2)
            mm.remove();
        else
            mm.setValue(mm.value() + "!");
    }
    CHECK(map.size() == 2 && map.value(3) == "c!");
    mm.toBack();
    mm.previous();
    mm.remove();
    CHECK(map.size() == 1 && map.contains(1));
}

static void testViews() {
    const QString s("  héllo wörld ");
    QStringView v(s);
    CHECK(v.size() == s.size());
    CHECK(v.trimmed().toString() == "héllo wörld");
    CHECK(v.mid(2, 5) == QStringView(u"héllo"));
    CHECK(v.mid(100).isEmpty() && v.mid(-1, 3).size() == 2);
    CHECK(v.trimmed().startsWith(QStringView(u"hé")) && v.contains(QStringView(u"wör")));
    CHECK(QStringView().isNull() && !QStringView(u"").isNull());
    CHECK(QStringView(u"abc") < QStringView(u"abd"));
    const QByteArray b("hello world");
    QByteArrayView bv(b);
    CHECK(bv.left(5) == QByteArrayView("hello") && bv.right(5).toByteArray() == "world");
    CHECK(bv.indexOf("o w") == 4 && bv.startsWith("hell") && bv.endsWith("rld") && !bv.startsWith("world"));
    CHECK(bv.mid(100).isEmpty());
}

static void testIdentifiers() {
    CHECK(QVersionNumber(1, 2, 3).toString() == "1.2.3");
    CHECK(QVersionNumber(1, 2) < QVersionNumber(1, 2, 1));
    CHECK(QVersionNumber(1, 10) > QVersionNumber(1, 9, 9));
    CHECK(QVersionNumber(1, 2, 0).normalized() == QVersionNumber(1, 2));
    qsizetype suffix = -1;
    const QVersionNumber parsed = QVersionNumber::fromString(QStringView(u"5.15.2-rc1"), &suffix);
    CHECK(parsed == QVersionNumber(5, 15, 2) && suffix == 6);
    CHECK(QVersionNumber::fromString(QStringView(u"1.")).segmentCount() == 1);
    CHECK(QVersionNumber::fromString(QStringView(u"x")).isNull());
    CHECK(QVersionNumber(1, 2).isPrefixOf(QVersionNumber(1, 2, 3)));

    // These all come from the VERSION file at the root of the repository.
    const QVersionNumber library = QLibraryInfo::version();
    CHECK(library.toString() == NT_EXPECTED_VERSION);
    CHECK(library == QVersionNumber::fromString(QString(NT_EXPECTED_VERSION)));
    CHECK(QString(qVersion()) == NT_EXPECTED_VERSION && QString(QT_VERSION_STR) == NT_EXPECTED_VERSION);
    CHECK(QT_VERSION == QT_VERSION_CHECK(library.majorVersion(), library.minorVersion(), library.microVersion()));
    CHECK(QString(QLibraryInfo::build()).contains(NT_EXPECTED_VERSION));

    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2, 3), QVersionNumber(1, 2, 4)) == QVersionNumber(1, 2));

    const QUuid ns = QUuid::fromString(QStringView(u"{6ba7b810-9dad-11d1-80b4-00c04fd430c8}"));
    CHECK(!ns.isNull());
    CHECK(ns.toString() == "{6ba7b810-9dad-11d1-80b4-00c04fd430c8}");
    CHECK(ns.toString(QUuid::WithoutBraces) == "6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    CHECK(ns.toString(QUuid::Id128) == "6ba7b8109dad11d180b400c04fd430c8");
    CHECK(QUuid::fromRfc4122(ns.toRfc4122()) == ns);
    CHECK(QUuid::createUuidV3(ns, QByteArray("www.example.com")).toString(QUuid::WithoutBraces) == kUuid3);
    CHECK(QUuid::createUuidV5(ns, QByteArray("www.example.com")).toString(QUuid::WithoutBraces) == kUuid5);
    CHECK(QUuid::createUuidV5(ns, QByteArray("x")).version() == QUuid::Sha1);
    const QUuid rnd = QUuid::createUuid();
    CHECK(rnd.version() == QUuid::Random && rnd.variant() == QUuid::DCE && rnd != QUuid::createUuid());
    CHECK(QUuid::fromString(QStringView(u"not-a-uuid")).isNull());
    CHECK(QUuid::fromString(QStringView(u"{6ba7b810-9dad-11d1-80b4-00c04fd430cg}")).isNull());
    CHECK(QUuid().isNull() && QUuid().variant() == QUuid::VarUnknown);

    QRandomGenerator a(42), b(42);
    for (int i = 0; i < 100; ++i)
        CHECK(a.generate() == b.generate());
    std::set<int> seen;
    for (int i = 0; i < 2000; ++i) {
        const int v = QRandomGenerator::global()->bounded(10, 20);
        CHECK(v >= 10 && v < 20);
        seen.insert(v);
    }
    CHECK(seen.size() == 10);
    CHECK(QRandomGenerator::system()->generate64() != QRandomGenerator::system()->generate64());
    const double d = QRandomGenerator::global()->generateDouble();
    CHECK(d >= 0.0 && d < 1.0);
}

static void testUrl() {
    // RFC 3986 section 5.4.
    const QUrl base("http://a/b/c/d;p?q");
    const struct { const char *ref, *expected; } cases[] = {
        {"g:h", "g:h"}, {"g", "http://a/b/c/g"}, {"./g", "http://a/b/c/g"}, {"g/", "http://a/b/c/g/"},
        {"/g", "http://a/g"}, {"//g", "http://g"}, {"?y", "http://a/b/c/d;p?y"}, {"g?y", "http://a/b/c/g?y"},
        {"#s", "http://a/b/c/d;p?q#s"}, {"g#s", "http://a/b/c/g#s"}, {"g?y#s", "http://a/b/c/g?y#s"},
        {";x", "http://a/b/c/;x"}, {"g;x", "http://a/b/c/g;x"}, {"g;x?y#s", "http://a/b/c/g;x?y#s"},
        {"", "http://a/b/c/d;p?q"}, {".", "http://a/b/c/"}, {"./", "http://a/b/c/"}, {"..", "http://a/b/"},
        {"../", "http://a/b/"}, {"../g", "http://a/b/g"}, {"../..", "http://a/"}, {"../../", "http://a/"},
        {"../../g", "http://a/g"},
        {"../../../g", "http://a/g"}, {"../../../../g", "http://a/g"}, {"/./g", "http://a/g"},
        {"/../g", "http://a/g"}, {"g.", "http://a/b/c/g."}, {".g", "http://a/b/c/.g"}, {"g..", "http://a/b/c/g.."},
        {"..g", "http://a/b/c/..g"}, {"./../g", "http://a/b/g"}, {"./g/.", "http://a/b/c/g/"},
        {"g/./h", "http://a/b/c/g/h"}, {"g/../h", "http://a/b/c/h"}, {"g;x=1/./y", "http://a/b/c/g;x=1/y"},
        {"g;x=1/../y", "http://a/b/c/y"}, {"g?y/./x", "http://a/b/c/g?y/./x"}, {"g?y/../x", "http://a/b/c/g?y/../x"},
        {"g#s/./x", "http://a/b/c/g#s/./x"}, {"g#s/../x", "http://a/b/c/g#s/../x"},
    };
    for (const auto &c : cases) {
        const QByteArray got = base.resolved(QUrl(c.ref)).toEncoded();
        if (got != QByteArray(c.expected))
            std::printf("  ref '%s' gave '%s'\n", c.ref, got.constData());
        CHECK(got == QByteArray(c.expected));
    }

    QUrl u("HTTP://Jo%20e:p%40ss@EXAMPLE.com:8080/a%20b/c.txt?x=1&y=%26#frag");
    CHECK(u.isValid() && u.scheme() == "http" && u.host() == "example.com" && u.port() == 8080);
    CHECK(u.userName() == "Jo e" && u.password() == "p@ss");
    CHECK(u.path() == "/a b/c.txt" && u.fileName() == "c.txt");
    CHECK(u.query() == "x=1&y=%26" && u.fragment() == "frag");
    CHECK(u.path(QUrl::FullyEncoded) == "/a%20b/c.txt");
    CHECK(u.toEncoded() == "http://Jo%20e:p%40ss@example.com:8080/a%20b/c.txt?x=1&y=%26#frag");
    CHECK(u.toString(QUrl::RemoveUserInfo) == "http://example.com:8080/a b/c.txt?x=1&y=%26#frag");
    CHECK(u.toString(QUrl::RemovePassword) == "http://Jo e@example.com:8080/a b/c.txt?x=1&y=%26#frag");
    CHECK(u.toEncoded(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveAuthority) == "http:/a%20b/c.txt");
    CHECK(QUrl("http://example.com").port(80) == 80);
    CHECK(QUrl("http://[::1]:99/x").host() == "::1" && QUrl("http://[::1]:99/x").port() == 99);
    CHECK(QUrl("http://[::1]:99/x").toEncoded() == "http://[::1]:99/x");

    // Encoding round trips, and % is literal in decoded mode.
    QUrl built;
    built.setScheme("http");
    built.setHost("example.com");
    built.setPath("/100% é/?#");
    CHECK(built.toEncoded() == "http://example.com/100%25%20%C3%A9/%3F%23");
    CHECK(built.path() == "/100% é/?#");
    CHECK(QUrl::fromEncoded(built.toEncoded()) == built);
    CHECK(QUrl("mailto:a@b.c").path() == "a@b.c" && QUrl("mailto:a@b.c").host().isEmpty());
    CHECK(QUrl("/just/a/path").isRelative());
    CHECK(QUrl::toPercentEncoding("a b/é", "/", "a") == "%61%20b/%C3%A9");
    CHECK(QUrl::fromPercentEncoding("a%20b%C3%A9") == "a bé");

    // Invalid input.
    CHECK(QUrl().isEmpty() && !QUrl().isValid());
    CHECK(!QUrl("http://example.com:99999/").isValid());
    CHECK(!QUrl("http://example.com:abc/").isValid());
    CHECK(!QUrl("http://exa mple.com/").isValid());
    CHECK(!QUrl("http://[zz::1]/").isValid());
    CHECK(!QUrl("http://a/b c", QUrl::StrictMode).isValid());
    CHECK(!QUrl("http://a/b%zz", QUrl::StrictMode).isValid());
    CHECK(QUrl("http://a/b c").toEncoded() == "http://a/b%20c");
    CHECK(QUrl("http://a/b%zz").toEncoded() == "http://a/b%25zz");
    CHECK(QUrl("http://a").errorString().isEmpty());
    QUrl badPort("http://a/");
    badPort.setPort(70000);
    CHECK(!badPort.isValid() && !badPort.errorString().isEmpty());

    const QUrl file = QUrl::fromLocalFile("/tmp/a b.txt");
    CHECK(file.toEncoded() == "file:///tmp/a%20b.txt" && file.isLocalFile() && file.toLocalFile() == "/tmp/a b.txt");
    CHECK(QUrl("http://a/b").toLocalFile().isEmpty());
    CHECK(QUrl("http://a/b").isParentOf(QUrl("http://a/b/c")) && !QUrl("http://a/b").isParentOf(QUrl("http://a/bc")));
    CHECK(QUrl("http://a/x/../y/").adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash).toEncoded() == "http://a/y");

    QUrlQuery q(QString("a=1&b=x%20y&a=2&flag&=e"));
    CHECK(q.queryItemValue("a") == "1" && q.queryItemValue("b") == "x y");
    CHECK(q.allQueryItemValues("a").size() == 2 && q.hasQueryItem("flag") && !q.hasQueryItem("z"));
    q.removeAllQueryItems("a");
    q.removeQueryItem("flag");
    CHECK(q.queryItems().size() == 2);
    QUrlQuery made;
    made.addQueryItem("k&=", "v é#");
    CHECK(made.query(QUrl::FullyEncoded) == "k%26%3D=v%20%C3%A9%23");
    CHECK(QUrlQuery(made.query(QUrl::FullyEncoded)) == made);
    CHECK(QUrlQuery(QUrl("http://a/?x=1")).queryItemValue("x") == "1");
    QUrlQuery fromList{{"a", "b"}};
    CHECK(fromList.query() == "a=b");
}

class TimerTarget : public QObject {
public:
    QBasicTimer timer;
    int hits = 0;

protected:
    void timerEvent(QTimerEvent *event) override {
        if (event->timerId() == timer.timerId() && ++hits == 2) {
            timer.stop();
            QCoreApplication::quit();
        }
    }
};

static void testBasicTimer() {
    TimerTarget t;
    t.timer.start(1, &t);
    CHECK(t.timer.isActive());
    QCoreApplication::exec();
    CHECK(t.hits == 2 && !t.timer.isActive());
}

int main(int argc, char **argv) {
    // Fixed so local-time checks exercise a real offset.
    setenv("TZ", "Europe/London", 1);
    QCoreApplication app(argc, argv);
    testHashes();
    testDates();
    testGeometry();
    testContainers();
    testViews();
    testIdentifiers();
    testUrl();
    testBasicTimer();
    if (g_failures == 0)
        std::printf("all core value tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
