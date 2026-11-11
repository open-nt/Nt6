// MIT License - see LICENSE
#include <QtCore>

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QString formatted(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static QString formatted(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const QString first = QString::vasprintf(fmt, ap);
    const QString second = QString::vasprintf(fmt, ap); // the list is left unchanged
    va_end(ap);
    CHECK(first == second);
    return first;
}

static void testString() {
    QString s = QStringLiteral("hello");
    CHECK(s.removeAt(1) == QStringLiteral("hllo"));
    CHECK(s.removeFirst() == QStringLiteral("llo"));
    CHECK(s.removeLast() == QStringLiteral("ll"));
    s.removeAt(7);
    s.removeAt(-1);
    CHECK(s == QStringLiteral("ll"));
    QString empty;
    empty.removeFirst().removeLast();
    CHECK(empty.isEmpty());

    s = QStringLiteral("abcdef");
    auto it = s.erase(s.cbegin() + 1, s.cbegin() + 3);
    CHECK(s == QStringLiteral("adef"));
    CHECK(*it == QChar('d'));
    it = s.erase(s.cbegin());
    CHECK(s == QStringLiteral("def") && *it == QChar('d'));
    it = s.erase(s.cend() - 1);
    CHECK(s == QStringLiteral("de") && it == s.end());

    s.push_front(QChar('c'));
    s.push_front(QStringLiteral("ab"));
    CHECK(s == QStringLiteral("abcde"));
    s.reserve(200);
    s.shrink_to_fit();
    CHECK(s == QStringLiteral("abcde"));
    CHECK(s.max_size() == QString::maxSize());
    CHECK(QString::maxSize() > 1000000);

    QString reversed;
    for (auto r = s.crbegin(); r != s.crend(); ++r)
        reversed.append(*r);
    CHECK(reversed == QStringLiteral("edcba"));
    *s.rbegin() = QChar('!');
    CHECK(s == QStringLiteral("abcd!"));
    const QString cs = s;
    CHECK(*cs.rbegin() == QChar('!'));
    CHECK(cs.rend() - cs.rbegin() == 5);
    CHECK(*(s.rend() - 1) == QChar('a'));

    CHECK(formatted("%d-%s-%05.1f", 42, "x", 2.5) == QStringLiteral("42-x-002.5"));
    CHECK(formatted("%s", "") == QString());
    CHECK(QString::asprintf("%c%c", 'o', 'k') == QStringLiteral("ok"));
}

static void testByteArray() {
    QByteArray b("hello");
    CHECK(b.removeAt(1) == "hllo");
    CHECK(b.removeFirst() == "llo");
    CHECK(b.removeLast() == "ll");
    b.removeAt(9);
    CHECK(b == "ll");
    QByteArray empty;
    empty.removeFirst().removeLast();
    CHECK(empty.isEmpty());

    b = "abcdef";
    auto it = b.erase(b.cbegin() + 1, b.cbegin() + 3);
    CHECK(b == "adef" && *it == 'd');
    it = b.erase(b.cbegin());
    CHECK(b == "def" && *it == 'd');
    b.push_front('c');
    b.push_front(QByteArray("ab"));
    CHECK(b == "abcdef");
    CHECK(b.constBegin() == b.cbegin() && b.constEnd() == b.cend());

    std::string reversed(b.crbegin(), b.crend());
    CHECK(reversed == "fedcba");
    *b.rbegin() = '!';
    CHECK(b == "abcde!");
    const QByteArray cb = b;
    CHECK(*cb.rbegin() == '!' && cb.rend() - cb.rbegin() == 6);
    CHECK(*(b.rend() - 1) == 'a');

    b.removeIf([](char c) { return c == 'b' || c == 'd'; });
    CHECK(b == "ace!");
    b.assign(3, 'x');
    CHECK(b == "xxx");
    b.assign(-2, 'x');
    CHECK(b.isEmpty() && !b.isNull());
    const std::vector<char> chars{'q', 't'};
    b.assign(chars.begin(), chars.end());
    CHECK(b == "qt");
    const char raw[] = "rawdata";
    b.setRawData(raw, 3);
    CHECK(b == "raw");
    b.setRawData(nullptr, 0);
    CHECK(b.isNull());
    b.reserve(100);
    b.shrink_to_fit();
    CHECK(b.max_size() == QByteArray::maxSize() && QByteArray::maxSize() > 1000000);
}

static void testAssociative() {
    QHash<QString, int> h;
    auto r = h.tryEmplace(QStringLiteral("a"), 1);
    CHECK(r.inserted && r.iterator.key() == QStringLiteral("a") && r.iterator.value() == 1);
    r = h.tryEmplace(QStringLiteral("a"), 2);
    CHECK(!r.inserted && r.iterator.value() == 1);
    r = h.tryInsert(QStringLiteral("b"), 5);
    CHECK(r.inserted && h.value(QStringLiteral("b")) == 5);
    CHECK(!h.tryInsert(QStringLiteral("b"), 6).inserted && h.value(QStringLiteral("b")) == 5);
    r = h.insertOrAssign(QStringLiteral("b"), 7);
    CHECK(!r.inserted && h.value(QStringLiteral("b")) == 7);
    CHECK(h.insertOrAssign(QStringLiteral("c"), 9).inserted);
    auto p = h.try_emplace(QStringLiteral("c"), 10);
    CHECK(!p.second && (*p.first).second == 9);
    p = h.insert_or_assign(QStringLiteral("d"), 11);
    CHECK(p.second && (*p.first).first == QStringLiteral("d") && (*p.first).second == 11);
    CHECK(h.size() == 4 && !h.empty());
    CHECK((QHash<int, int>().empty()));
    CHECK(h.bucket_count() >= 4 && h.max_bucket_count() >= h.bucket_count());
    CHECK(h.load_factor() > 0.0f && h.max_load_factor() >= h.load_factor());

    QStringList keys;
    for (auto k = h.keyBegin(); k != h.keyEnd(); ++k)
        keys.append(*k);
    keys.sort();
    CHECK(keys == QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")}));
    CHECK(h.keyBegin()->size() == 1);
    auto range = h.equal_range(QStringLiteral("a"));
    CHECK(range.first != range.second && range.first.value() == 1);
    range = h.equal_range(QStringLiteral("zz"));
    CHECK(range.first == range.second);

    QMultiHash<int, int> mh;
    mh.insert(1, 10);
    mh.insert(1, 11);
    QMultiHash<int, int> other;
    other.insert(1, 12);
    other.insert(2, 20);
    mh.unite(other);
    CHECK(mh.size() == 4 && mh.count(1) == 3);
    mh.unite(mh);
    CHECK(mh.size() == 8);
    const QMultiHash<int, int> &cmh = mh;
    auto mr = cmh.equal_range(1);
    CHECK(std::distance(mr.first, mr.second) == 6);
    mh.emplaceReplace(2, 21);
    CHECK(mh.count(2) == 2 && mh.values(2).contains(21));
    mh.emplaceReplace(3, 30);
    CHECK(mh.value(3) == 30);
    CHECK(mh.bucket_count() > 0 && mh.load_factor() > 0.0f && mh.max_load_factor() > 0.0f && mh.max_bucket_count() > 0);

    QMap<int, QString> m{{2, QStringLiteral("two")}, {1, QStringLiteral("one")}};
    const std::map<int, QString> sm = m.toStdMap();
    CHECK(sm.size() == 2 && sm.begin()->second == QStringLiteral("one"));
    QList<int> mapKeys(m.keyBegin(), m.keyEnd());
    CHECK(mapKeys == QList<int>({1, 2}));
    CHECK(!m.empty());
    const std::map<int, QString> moved = std::move(m).toStdMap();
    CHECK(moved.size() == 2);

    QMultiMap<int, int> mm;
    mm.insert(1, 1);
    mm.insert(1, 2);
    QMultiMap<int, int> mm2;
    mm2.insert(0, 0);
    mm.unite(mm2);
    CHECK(mm.size() == 3 && mm.firstKey() == 0);
    const std::multimap<int, int> smm = mm.toStdMultiMap();
    CHECK(smm.size() == 3 && smm.count(1) == 2);
    auto mmr = mm.equal_range(1);
    CHECK(std::distance(mmr.first, mmr.second) == 2);

    QSet<int> set;
    CHECK(set.empty());
    set.insert(1);
    CHECK(!set.empty());
}

static void testSequences() {
    QList<QString> l{QStringLiteral("b")};
    QString &front = l.emplaceFront(3, QChar('a'));
    CHECK(front == QStringLiteral("aaa") && l.size() == 2 && l.first() == QStringLiteral("aaa"));
    l.emplace_front(QStringLiteral("z"));
    CHECK(l.first() == QStringLiteral("z") && l.last() == QStringLiteral("b"));
    CHECK(QList<int>::maxSize() > 1000000 && QList<int>::maxSize() >= QList<qint64>::maxSize());
    CHECK((QVarLengthArray<int, 4>::maxSize()) == (QVarLengthArray<int, 4>::max_size()));

    int values[] = {1, 2, 3, 4, 5};
    QSpan<int> span(values);
    span.chop(1);
    CHECK(span.size() == 4 && span.back() == 4);
    span.slice(1);
    CHECK(span.size() == 3 && span.front() == 2);
    span.slice(1, 1);
    CHECK(span.size() == 1 && span.front() == 3);
}

static void testViews() {
    QUtf8StringView u("abcdef");
    CHECK(u.left(2) == QUtf8StringView("ab") && u.left(-1) == u && u.left(99) == u);
    CHECK(u.right(2) == QUtf8StringView("ef") && u.right(99) == u);
    CHECK(u.mid(2) == QUtf8StringView("cdef") && u.mid(2, 2) == QUtf8StringView("cd"));
    CHECK(u.mid(4, 99) == QUtf8StringView("ef") && u.mid(99).isEmpty() && u.mid(-2, 4) == QUtf8StringView("ab"));
    CHECK(u.mid(-1) == u);
    CHECK(std::string(u.rbegin(), u.rend()) == "fedcba");
    CHECK(std::string(u.crbegin(), u.crend()) == "fedcba");
    QUtf8StringView sliced = u;
    CHECK(sliced.slice(2) == QUtf8StringView("cdef"));
    CHECK(sliced.slice(1, 2) == QUtf8StringView("de"));
    CHECK(QUtf8StringView::max_size() == QUtf8StringView::maxSize() && QUtf8StringView::maxSize() > 1000000);
    const char array[4] = {'a', 'b', 'c', '\0'};
    CHECK(QUtf8StringView::fromArray(array).size() == 4);

    const QString text = QStringLiteral("abcdef");
    QAnyStringView a(text);
    CHECK(a.left(2) == QAnyStringView("ab") && a.left(-1) == a && a.left(99) == a);
    CHECK(a.right(2) == QAnyStringView("ef") && a.right(99) == a);
    CHECK(a.mid(2) == QAnyStringView("cdef") && a.mid(2, 2) == QAnyStringView("cd") && a.mid(99).isEmpty());
    QAnyStringView a8(u);
    CHECK(a8.left(3) == QAnyStringView("abc") && a8.mid(4, 99) == QAnyStringView("ef") &&
          a8.right(1) == QAnyStringView("f"));
    CHECK(QAnyStringView::max_size() > 1000000);

    QLatin1StringView l("abc");
    CHECK(l.constBegin() == l.begin() && l.constEnd() == l.end() && l.constData() == l.data());
    CHECK(QLatin1StringView::maxSize() == QLatin1StringView::max_size());
    CHECK(QStringView::maxSize() == QStringView::max_size());
    CHECK(QByteArrayView::maxSize() == QByteArrayView::max_size());
}

struct Shared : QSharedData {
    int value = 0;
};

class TimerObject : public QObject {
public:
    QBasicTimer timer;
    QBasicTimer other;
    bool matched = false;
    bool matchedOther = true;
    Qt::TimerId seen = Qt::TimerId::Invalid;

protected:
    void timerEvent(QTimerEvent *e) override {
        matched = e->matches(timer);
        matchedOther = e->matches(other);
        seen = e->id();
        timer.stop();
        QCoreApplication::quit();
    }
};

static void testSmallMembers() {
    QSharedDataPointer<Shared> sd(new Shared);
    QSharedDataPointer<Shared> copy = sd;
    const QSharedDataPointer<Shared> &constRef = sd;
    CHECK(constRef.get() == copy.constData());
    sd.get()->value = 3; // detaches
    CHECK(copy->value == 0 && constRef.get() != copy.constData());
    QExplicitlySharedDataPointer<Shared> ex(new Shared);
    CHECK(ex.get() == ex.data());

    CHECK(QRect(1, 2, 3, 4).toRectF() == QRectF(1, 2, 3, 4));
    CHECK(QRect::span(QPoint(5, 1), QPoint(2, 7)) == QRect(QPoint(2, 1), QPoint(5, 7)));
    CHECK(QRect::span(QPoint(1, 1), QPoint(1, 1)) == QRect(1, 1, 1, 1));
    CHECK(QLine(1, 2, 3, 4).toLineF() == QLineF(1, 2, 3, 4));
    CHECK(QMargins(1, 2, 3, 4).toMarginsF() == QMarginsF(1, 2, 3, 4));

    QObject object;
    {
        QSignalBlocker blocker(&object);
        CHECK(object.signalsBlocked());
        blocker.dismiss();
    }
    CHECK(object.signalsBlocked());
    object.blockSignals(false);

    CHECK(QThread::currentThread()->isCurrentThread());
    QThread otherThread;
    CHECK(!otherThread.isCurrentThread());

    QTimer timer;
    CHECK(timer.id() == Qt::TimerId::Invalid);
    timer.start(1000);
    CHECK(timer.id() != Qt::TimerId::Invalid && qToUnderlying(timer.id()) == timer.timerId());
    timer.stop();

    TimerObject to;
    CHECK(to.timer.id() == Qt::TimerId::Invalid);
    to.timer.start(1, &to);
    const Qt::TimerId id = to.timer.id();
    CHECK(id != Qt::TimerId::Invalid && qToUnderlying(id) == to.timer.timerId());
    QCoreApplication::exec();
    CHECK(to.matched && !to.matchedOther && to.seen == id);

    QTemporaryDir dir;
    const QString lockName = dir.filePath(QStringLiteral("more.lock"));
    QLockFile lock(lockName);
    CHECK(lock.fileName() == lockName);
    CHECK(lock.staleLockTimeAsDuration() == std::chrono::milliseconds(lock.staleLockTime()));
    lock.setStaleLockTime(std::chrono::seconds(5));
    CHECK(lock.staleLockTime() == 5000 && lock.staleLockTimeAsDuration() == std::chrono::seconds(5));
    CHECK(lock.tryLock(std::chrono::milliseconds(100)));
    CHECK(lock.isLocked());
    QLockFile second(lockName);
    CHECK(!second.tryLock(std::chrono::milliseconds(20)));
    lock.unlock();
    CHECK(second.tryLock(std::chrono::milliseconds(-1)));
    second.unlock();

    CHECK(QDebug::toString(42) == QStringLiteral("42"));
    CHECK(QDebug::toString(QStringLiteral("a b")) == QStringLiteral("\"a b\""));
    CHECK(QDebug::toString(QPoint(1, 2)) == QStringLiteral("QPoint(1,2)"));
    CHECK(QDebug::toBytes(QList<int>{1, 2}) == QDebug::toString(QList<int>{1, 2}).toUtf8());
    QString out;
    {
        QDebug dbg(&out);
        CHECK(dbg.quoteStrings());
        dbg.setQuoteStrings(false);
        CHECK(!dbg.quoteStrings());
        dbg << QStringLiteral("plain");
        dbg.quote();
        CHECK(dbg.quoteStrings());
    }
    CHECK(out.trimmed() == QStringLiteral("plain"));
}

static void testHashInto() {
    const QByteArray data("The quick brown fox");
    const QByteArray expected = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    std::array<char, 64> buffer{};
    QByteArrayView view = QCryptographicHash::hashInto(QSpan<char>(buffer), data, QCryptographicHash::Sha256);
    CHECK(view.size() == 32 && view.data() == buffer.data() && view.toByteArray() == expected);
    std::array<unsigned char, 32> ubuffer{};
    view = QCryptographicHash::hashInto(QSpan<unsigned char>(ubuffer), data, QCryptographicHash::Sha256);
    CHECK(view.toByteArray() == expected);
    std::array<std::byte, 32> bbuffer{};
    view = QCryptographicHash::hashInto(QSpan<std::byte>(bbuffer), data, QCryptographicHash::Sha256);
    CHECK(view.toByteArray() == expected);
    std::array<char, 31> small{};
    view = QCryptographicHash::hashInto(QSpan<char>(small), data, QCryptographicHash::Sha256);
    CHECK(view.isNull());

    const QByteArray mac = QMessageAuthenticationCode::hash(data, "key", QCryptographicHash::Sha1);
    view = QMessageAuthenticationCode::hashInto(QSpan<char>(buffer), data, "key", QCryptographicHash::Sha1);
    CHECK(view.size() == 20 && view.toByteArray() == mac);
    view = QMessageAuthenticationCode::hashInto(QSpan<char>(small).first(19), data, "key", QCryptographicHash::Sha1);
    CHECK(view.isNull());
}

struct Counted {
    explicit Counted(int *c) : counter(c) {}
    ~Counted() { ++*counter; }
    int *counter;
};

enum class Colour : unsigned char { Red = 3, Green = 200 };

static void testGlobals() {
    CHECK(qstrlen(nullptr) == 0 && qstrlen("") == 0 && qstrlen("abc") == 3);
    CHECK(qstrnlen("abcdef", 3) == 3 && qstrnlen("ab", 9) == 2 && qstrnlen(nullptr, 4) == 0);
    char buffer[8];
    CHECK(qstrcpy(buffer, "abc") == buffer && std::strcmp(buffer, "abc") == 0);
    CHECK(qstrcpy(buffer, nullptr) == nullptr);
    std::memset(buffer, 'x', sizeof(buffer));
    CHECK(qstrncpy(buffer, "abcdefghij", 4) == buffer && std::strcmp(buffer, "abc") == 0);
    std::memset(buffer, 'x', sizeof(buffer));
    qstrncpy(buffer, "ab", sizeof(buffer));
    CHECK(std::strcmp(buffer, "ab") == 0 && buffer[7] == '\0');
    CHECK(qstrncpy(buffer, nullptr, 4) == nullptr);
    buffer[0] = 'k';
    CHECK(qstrncpy(buffer, "abc", 0) == buffer && buffer[0] == 'k');

    CHECK(qstrcmp("a", "a") == 0 && qstrcmp("a", "b") < 0 && qstrcmp("b", "a") > 0);
    CHECK(qstrcmp(static_cast<const char *>(nullptr), static_cast<const char *>(nullptr)) == 0);
    CHECK(qstrcmp(static_cast<const char *>(nullptr), "a") < 0 && qstrcmp("a", static_cast<const char *>(nullptr)) > 0);
    CHECK(qstrcmp(QByteArray("a"), QByteArray("b")) < 0 && qstrcmp(QByteArray("b"), QByteArray("b")) == 0);
    CHECK(qstrcmp(QByteArray("b"), "a") > 0 && qstrcmp("a", QByteArray("b")) < 0);
    CHECK(qstrcmp(QByteArray(), static_cast<const char *>(nullptr)) == 0 &&
          qstrcmp(QByteArray("a"), static_cast<const char *>(nullptr)) > 0);
    CHECK(qstrncmp("abcd", "abxx", 2) == 0 && qstrncmp("abcd", "abxx", 3) < 0 && qstrncmp(nullptr, "a", 1) < 0);
    CHECK(qstricmp("HeLLo", "hello") == 0 && qstricmp("abc", "ABD") < 0 && qstricmp("abcd", "ABC") > 0);
    CHECK(qstricmp(nullptr, nullptr) == 0 && qstricmp(nullptr, "a") < 0 && qstricmp("a", nullptr) > 0);
    CHECK(qstrnicmp("ABCD", "abxx", 2) == 0 && qstrnicmp("ABCD", "abxx", 3) < 0 && qstrnicmp("ab", "AB", 9) == 0);
    CHECK(qstrnicmp("ABCxx", 3, "abc") == 0 && qstrnicmp("ABCxx", 3, "abcd") < 0 &&
          qstrnicmp("ABCD", 4, "abcz", 3) > 0);
    CHECK(qstrnicmp(nullptr, 0, "") == 0 && qstrnicmp(nullptr, 0, "a") < 0 && qstrnicmp("a", 1, nullptr, 0) > 0);

    int deleted = 0;
    QList<Counted *> objects{new Counted(&deleted), new Counted(&deleted), nullptr};
    qDeleteAll(objects);
    CHECK(deleted == 2 && objects.size() == 3);
    std::vector<Counted *> more{new Counted(&deleted)};
    qDeleteAll(more.begin(), more.end());
    CHECK(deleted == 3);

    const QString name = QStringLiteral("n");
    auto pair = qMakePair(1, name);
    CHECK((std::is_same_v<decltype(pair), std::pair<int, QString>>));
    CHECK(pair.first == 1 && pair.second == name);
    CHECK(qToUnderlying(Colour::Green) == 200);
    CHECK((std::is_same_v<decltype(qToUnderlying(Colour::Red)), unsigned char>));
    CHECK(qIsNull(0.0) && qIsNull(-0.0) && !qIsNull(1e-300) && qIsNull(0.0f) && !qIsNull(1e-30f));

    const QString previous = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("more-tests"));
    CHECK(qAppName() == QStringLiteral("more-tests"));
    QCoreApplication::setApplicationName(previous);
    CHECK(QCoreApplication::instanceExists() && !QCoreApplication::startingUp() && !QCoreApplication::closingDown());
}

static void testCompress() {
    const QByteArray text = QByteArray("Nt compresses this text. ").repeated(200);
    const QByteArray packed = qCompress(text);
    if (packed.isEmpty()) {
        // Built without zlib: both functions report failure with an empty array.
        CHECK(qUncompress(QByteArray("\0\0\0\1x", 5)).isEmpty());
        return;
    }
    CHECK(packed.size() < text.size());
    const auto size = static_cast<quint32>(text.size());
    CHECK(static_cast<uchar>(packed[0]) == (size >> 24) && static_cast<uchar>(packed[3]) == (size & 0xff));
    CHECK(static_cast<uchar>(packed[4]) == 0x78); // a zlib stream follows the size
    CHECK(qUncompress(packed) == text);
    CHECK(qUncompress(qCompress(text, 0)) == text);
    CHECK(qCompress(text, 9).size() <= qCompress(text, 1).size());
    CHECK(qUncompress(qCompress(text, 99)) == text);
    const QByteArray none = qCompress(QByteArray(""));
    CHECK(!none.isEmpty() && qUncompress(none).isEmpty());
    CHECK(qUncompress(qCompress(reinterpret_cast<const uchar *>("abc"), 3)) == "abc");

    // A wrong size in the header is only a hint.
    QByteArray lying = packed;
    lying[0] = lying[1] = lying[2] = '\0';
    lying[3] = '\1';
    CHECK(qUncompress(lying) == text);
    // Damaged and truncated input fails instead of returning part of the data.
    CHECK(qUncompress(packed.left(packed.size() / 2)).isEmpty());
    CHECK(qUncompress(packed.left(4)).isEmpty());
    CHECK(qUncompress(QByteArray("ab")).isEmpty());
    CHECK(qUncompress(QByteArray()).isEmpty());
    QByteArray damaged = packed;
    damaged[5] = static_cast<char>(damaged[5] ^ 0x55);
    damaged[packed.size() / 2] = static_cast<char>(damaged[packed.size() / 2] ^ 0x55);
    CHECK(qUncompress(damaged) != text);
    CHECK(qCompress(static_cast<const uchar *>(nullptr), 0).isEmpty());
}

int main(int argc, char **argv) {
    CHECK(QCoreApplication::startingUp() && !QCoreApplication::instanceExists() && !QCoreApplication::closingDown());
    {
        QCoreApplication app(argc, argv);
        testString();
        testByteArray();
        testAssociative();
        testSequences();
        testViews();
        testSmallMembers();
        testHashInto();
        testGlobals();
        testCompress();
    }
    CHECK(QCoreApplication::closingDown() && !QCoreApplication::instanceExists() && !QCoreApplication::startingUp());

    if (g_failures) {
        std::printf("%d more-core test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all more-core tests passed");
    return 0;
}
