// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qbytearrayalgorithms.h>
#include <QPair>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
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

static void testBoolList() {
    QList<bool> flags{true, false, true};
    CHECK(flags.size() == 3 && flags.at(0) && !flags.at(1) && flags[2] && flags.first() && flags.last());
    // The items are real bools: references, pointers and an array behind them.
    bool &second = flags[1];
    second = true;
    bool *raw = flags.data();
    CHECK(flags.at(1) && raw[1] && raw + 3 == flags.data() + flags.size() && flags.constData() == raw);
    raw[1] = false;
    for (bool &flag : flags)
        flag = !flag;
    CHECK(flags == QList<bool>({false, true, false}));
    QSpan<const bool> span(flags.constData(), flags.size());
    CHECK(span.size() == 3 && span[1]);

    flags.append(true);
    flags << false << true;
    flags.prepend(true);
    flags.insert(1, 2, false);
    CHECK(flags == QList<bool>({true, false, false, false, true, false, true, false, true}));
    CHECK(flags.count(true) == 4 && flags.contains(false) && flags.indexOf(true, 1) == 4 && flags.lastIndexOf(false) == 7);
    CHECK(flags.removeAll(false) == 5 && flags == QList<bool>(4, true) && !flags.contains(false));
    flags.resize(6);
    CHECK(flags.size() == 6 && !flags.at(5) && flags.takeLast() == false && flags.takeFirst() == true);
    flags.removeAt(0);
    flags.remove(0, 2);
    CHECK(flags == QList<bool>{false} && !flags.isEmpty());
    flags.emplaceBack(true);
    flags.emplaceFront(true);
    flags.insert(flags.begin() + 1, true);
    flags.replace(1, false);
    flags.move(0, 3);
    flags.swapItemsAt(0, 1);
    CHECK(flags == QList<bool>({false, false, true, true}));
    // Appending a list to itself, and inserting its own items.
    flags.append(flags);
    CHECK(flags.size() == 8 && flags.mid(4) == flags.mid(0, 4) && flags.first(2) == QList<bool>(2, false));
    flags.erase(flags.begin(), flags.begin() + 6);
    flags.fill(true, 3);
    CHECK(flags == QList<bool>(3, true));
    flags.assign({false, true});
    CHECK(flags.sliced(1) == QList<bool>{true} && flags.value(9, true) && !flags.value(0, true));
    flags.reserve(100);
    CHECK(flags.capacity() >= 100);
    flags.squeeze();
    std::sort(flags.begin(), flags.end(), std::greater<bool>());
    CHECK(flags == QList<bool>({true, false}) && std::count(flags.cbegin(), flags.cend(), true) == 1);
    CHECK(std::vector<bool>(flags.rbegin(), flags.rend()) == std::vector<bool>({false, true}));
    CHECK(flags.toStdVector() == std::vector<bool>({true, false}));
    CHECK((QList<bool>{false, true} < QList<bool>{true} && QList<bool>{true} != QList<bool>{true, true}));
    const std::vector<bool> packed{true, true, false};
    const QList<bool> fromPacked(packed.begin(), packed.end());
    CHECK(fromPacked.size() == 3 && fromPacked.at(1) && !fromPacked.at(2) && QList<bool>(2).count(false) == 2);
    QList<bool> other{true};
    other.swap(flags);
    CHECK(flags == QList<bool>{true} && other.size() == 2);
    flags.clear();
    CHECK(flags.isEmpty());

    // In the containers and the types that are built on lists.
    const QVariant variant = QVariant::fromValue(QList<bool>{true, false});
    CHECK(variant.value<QList<bool>>() == QList<bool>({true, false}));
    QByteArray wire;
    {
        QDataStream out(&wire, QIODevice::WriteOnly);
        out << QList<bool>{true, false, true};
    }
    QList<bool> read;
    QDataStream in(wire);
    in >> read;
    CHECK(read == QList<bool>({true, false, true}));
    QMap<QString, QList<bool>> map;
    map[QStringLiteral("k")] << true;
    CHECK(map.value(QStringLiteral("k")).size() == 1 && QStack<bool>().isEmpty() && QQueue<bool>().isEmpty());
    QVector<bool> vector(3, true);
    CHECK(vector.size() == 3 && QDebug::toString(vector).contains(QStringLiteral("true")));
}

// What a project written for Qt 6 expects of the basic types.

static_assert(std::is_same_v<QStringList, QList<QString>>, "QStringList is QList<QString>");
static_assert(std::is_same_v<QByteArrayList, QList<QByteArray>>, "QByteArrayList is QList<QByteArray>");
static_assert(std::is_same_v<qint64, long long> && std::is_same_v<quint64, unsigned long long>, "qint64 is long long");
static_assert(std::is_same_v<QPair<int, QString>, std::pair<int, QString>>, "QPair is std::pair");
static_assert(sizeof(void *) != 8 || (std::is_same_v<qsizetype, qint64> && std::is_same_v<qptrdiff, qint64> &&
                                      std::is_same_v<qintptr, qint64> && std::is_same_v<quintptr, quint64>),
              "the integers for sizes and pointers are qint64 and quint64 on a 64-bit platform");
static_assert(sizeof(qsizetype) == sizeof(std::size_t) && sizeof(quintptr) == sizeof(void *), "as wide as a size");
static_assert(!std::is_convertible_v<QString, QStringList>, "a string is not a list of strings");
static constexpr QByteArrayView g_constantView("abc");
static_assert(g_constantView.size() == 3, "a view of a literal is a constant expression");

static int takesList(const QList<QString> &list) { return static_cast<int>(list.size()); }
static QList<QString> givesList() { return {QStringLiteral("x"), QStringLiteral("y")}; }

static void testQtCompatibility(const QString &dir) {
    using namespace Qt::StringLiterals;
    const QString a = QStringLiteral("alpha");
    const QString b = QStringLiteral("beta");

    // A row of strings appended to a table of rows: one row, not two.
    QList<QStringList> rows;
    rows.append({a, b});
    rows.append({a});
    rows << QStringList{b, a, b};
    CHECK(rows.size() == 3 && rows.at(0).size() == 2 && rows.at(1).size() == 1 && rows.at(2).size() == 3);
    QList<QList<QString>> same = rows;
    CHECK(same == rows);
    const QStringList fromFunction = givesList();
    CHECK(takesList(fromFunction) == 2 && takesList(QStringList{a}) == 1);
    CHECK(QStringList(a) == QStringList{a} && QStringList("literal").first() == QStringLiteral("literal"));
    CHECK(QStringList(a).size() == 1 && QList<QString>(3).size() == 3);

    // The string functions are those of every QList<QString>.
    QList<QString> list{QStringLiteral("b"), QStringLiteral("A"), QStringLiteral("c"), QStringLiteral("b")};
    CHECK(list.join(QChar(',')) == QStringLiteral("b,A,c,b") && list.join(QStringLiteral("--")).size() == 10);
    CHECK(list.join(u"+"_s) == QStringLiteral("b+A+c+b") && list.join(QStringView(u"")) == QStringLiteral("bAcb"));
    CHECK(list.contains(QStringLiteral("A")) && !list.contains(QStringLiteral("a")));
    CHECK(list.contains(QStringLiteral("a"), Qt::CaseInsensitive) && list.contains("c") && !list.contains("d"));
    CHECK(list.contains(QLatin1StringView("b")) && list.contains(QStringView(u"c")) &&
          list.contains(u"a"_s, Qt::CaseInsensitive));
    CHECK(list.contains(QLatin1StringView("B"), Qt::CaseInsensitive) && !list.contains(QLatin1StringView("B")));
    CHECK(list.indexOf(QStringLiteral("b")) == 0 && list.indexOf(QStringLiteral("b"), 1) == 3 &&
          list.indexOf("zz") == -1);
    CHECK(list.indexOf(QStringLiteral("a"), 0, Qt::CaseInsensitive) == 1 && list.indexOf(QLatin1StringView("c")) == 2);
    CHECK(list.lastIndexOf(QStringLiteral("b")) == 3 && list.lastIndexOf(QStringLiteral("b"), 2) == 0);
    CHECK(list.lastIndexOf(QStringLiteral("B"), -1, Qt::CaseInsensitive) == 3 && list.count(QStringLiteral("b")) == 2);
    CHECK(list.indexOf(QRegularExpression(QStringLiteral("[A-Z]"))) == 1);
    CHECK(list.filter(QStringLiteral("b")).size() == 2 &&
          list.filter(QRegularExpression(QStringLiteral("^[ac]$"))).size() == 1);
    QStringList sorted = list;
    sorted.sort();
    CHECK(sorted == QStringList({QStringLiteral("A"), QStringLiteral("b"), QStringLiteral("b"), QStringLiteral("c")}));
    sorted.sort(Qt::CaseInsensitive);
    CHECK(sorted.first() == QStringLiteral("A") && sorted.removeDuplicates() == 1 && sorted.size() == 3);
    CHECK(sorted.replaceInStrings(QStringLiteral("b"), QStringLiteral("bee")).at(1) == QStringLiteral("bee"));
    CHECK(&sorted.replaceInStrings(QStringLiteral("x"), QStringLiteral("y")) == &sorted);
    QVariant variant(list);
    CHECK(variant.typeId() == QMetaType::QStringList && variant.toStringList() == list);
    CHECK(variant.value<QList<QString>>() == list && QVariant::fromValue(list).typeId() == QMetaType::QStringList);

    // Comparing with a value of another type.
    const QList<QLatin1StringView> names{"get"_L1, "set"_L1, "get"_L1};
    const QString method = QStringLiteral("set");
    CHECK(names.contains(method) && !names.contains(QStringLiteral("put")) && names.indexOf(method) == 1);
    CHECK(names.lastIndexOf(QStringLiteral("get")) == 2 && names.count(QStringLiteral("get")) == 2);
    QList<QLatin1StringView> fewer = names;
    CHECK(fewer.removeAll(QStringLiteral("get")) == 2 && fewer.size() == 1 && !fewer.removeOne(QStringLiteral("get")));
    CHECK(fewer.removeOne(method) && fewer.isEmpty());
    QList<int> numbers{1, 2, 1, 3};
    CHECK(numbers.contains(2L) && numbers.indexOf(3LL) == 3 && numbers.contains({}) == false);
    // An item of the list itself, as what to remove.
    CHECK(numbers.removeAll(numbers.first()) == 2 && numbers == QList<int>({2, 3}));
    QStringList words{a, b, a};
    CHECK(words.removeAll(words.at(0)) == 2 && words == QStringList{b});

    // Lists of byte arrays.
    const QByteArrayList parts{"a", "bc", ""};
    CHECK(parts.join() == "abc" && parts.join(',') == "a,bc," && parts.join(QByteArrayView("--")) == "a--bc--");
    CHECK(parts.join(QByteArray("::")) == "a::bc::" && parts.join("+") == "a+bc+" &&
          QByteArrayList().join(',').isEmpty());
    CHECK(QByteArrayList{"one"}.join(',') == "one" && parts.contains("bc") && parts.indexOf("bc") == 1);
    CHECK(QByteArray("x,y").split(',').join(';') == "x;y");

    // Views of bytes.
    const QByteArray bytes("mid");
    CHECK(QByteArrayView("pre-") + bytes == "pre-mid" && bytes + QByteArrayView("-post") == "mid-post");
    CHECK("pre-" + bytes == "pre-mid" && bytes + "-post" == "mid-post"); // as before
    CHECK(QByteArray("abc"_L1) == "abc" && QByteArray(QLatin1StringView()).isNull());
    CHECK(QByteArray(QByteArrayView("xyz", 2)) == "xy" && QByteArray(QByteArrayView()).isNull());
    const std::string stdString = "std";
    const std::vector<char> vector{'v', 'e', 'c'};
    const std::array<char, 2> array{'a', 'r'};
    CHECK(QByteArrayView(stdString) == QByteArrayView("std") && QByteArrayView(vector).size() == 3);
    CHECK(QByteArrayView(array).toByteArray() == "ar" && QByteArrayView("abc"_L1).size() == 3);
    CHECK(QByteArray(QByteArrayView(vector)) == "vec");
    const std::string_view stdView = "caf\xe9";
    CHECK(QString::fromLatin1(stdView) == QString(QChar('c')) + QChar('a') + QChar('f') + QChar(0xe9));
    CHECK(QString::fromLatin1(QByteArrayView("ab")) == QStringLiteral("ab") &&
          QString::fromLatin1(QByteArrayView()).isEmpty());
    QByteArray detached("d");
    detached.detach();
    CHECK(detached == "d" && detached.isDetached());

    // tokenize() needs no other header than that of the string or the view.
    QStringList tokens;
    for (QStringView token : QStringView(u"a.b..c").tokenize(u'.'))
        tokens.append(token.toString());
    CHECK(tokens.size() == 4 && tokens.join(QChar('|')) == QStringLiteral("a|b||c"));
    int count = 0;
    for (QStringView token : a.tokenize(u"l"_s, Qt::SkipEmptyParts))
        count += static_cast<int>(token.size());
    CHECK(count == 4);

    // qint64 is long long.
    char text[64];
    const qint64 big = Q_INT64_C(-9007199254740993);
    const quint64 ubig = Q_UINT64_C(18446744073709551615);
    std::snprintf(text, sizeof(text), "%lld/%llu", big, ubig);
    CHECK(std::strcmp(text, "-9007199254740993/18446744073709551615") == 0);
    CHECK(QString::number(big) == QStringLiteral("-9007199254740993") && QVariant(big).typeId() == QMetaType::LongLong);
    QByteArray wire;
    {
        QDataStream out(&wire, QIODevice::WriteOnly);
        out << big << ubig << qint64(1) << quint64(2);
    }
    QDataStream in(wire);
    qint64 readBig = 0;
    quint64 readUbig = 0;
    in >> readBig >> readUbig;
    CHECK(wire.size() == 32 && readBig == big && readUbig == ubig);

    // QPair, and the headers of the endian and byte array functions.
    const QPair<int, QString> pair = qMakePair(7, a);
    CHECK(pair.first == 7 && pair.second == a);
    CHECK(qToBigEndian<quint16>(0x1234) == (QSysInfo::ByteOrder == QSysInfo::LittleEndian ? 0x3412 : 0x1234));
    CHECK(qstrlen("four") == 4);

    // A size is a qint64, so it goes where a qint64 goes: into JSON, a variant or a format string.
    const QStringList sized{a, b};
    QJsonObject counts;
    counts[QStringLiteral("n")] = sized.size();
    QJsonArray sizes;
    sizes.append(sized.size());
    sizes.append(a.size());
    CHECK(QJsonValue(sized.size()).toInteger() == 2 && counts.value(QStringLiteral("n")).toInt() == 2);
    CHECK(sizes.at(1).toInt() == 5 && QVariant(sized.size()).typeId() == QMetaType::LongLong);
    std::snprintf(text, sizeof(text), "%lld", sized.size());
    CHECK(std::strcmp(text, "2") == 0 && QString::number(sized.size()) == QStringLiteral("2"));

    // The members of a JSON object, as pairs.
    QJsonObject object{{QStringLiteral("one"), 1}, {QStringLiteral("two"), 2}};
    int sum = 0;
    QStringList keys;
    for (auto [key, value] : object.asKeyValueRange()) {
        keys.append(key.toString());
        sum += value.toInt();
        value = value.toInt() * 10;
    }
    static_assert(std::is_same_v<decltype((*object.keyValueBegin()).first), QAnyStringView>);
    static_assert(std::is_same_v<decltype((*object.constKeyValueBegin()).first), QAnyStringView>);
    CHECK(object.begin().keyView() == QStringLiteral("one") && object.constBegin().keyView().toString() == QStringLiteral("one"));
    CHECK(sum == 3 && keys == QStringList({QStringLiteral("one"), QStringLiteral("two")}));
    const QJsonObject &constObject = object;
    sum = 0;
    for (auto [key, value] : constObject.asKeyValueRange())
        sum += value.toInt() + static_cast<int>(key.size());
    CHECK(sum == 36);
    sum = 0;
    for (auto [key, value] : QJsonObject{{QStringLiteral("k"), 5}}.asKeyValueRange())
        sum += value.toInt();
    CHECK(sum == 5 && std::distance(constObject.constKeyValueBegin(), constObject.constKeyValueEnd()) == 2);
    CHECK((*object.keyValueBegin()).first == QStringLiteral("one") && object.keyValueBegin() != object.keyValueEnd());
    QMap<QString, int> map{{a, 1}};
    QHash<QString, int> hash{{b, 2}};
    for (auto [key, value] : map.asKeyValueRange())
        CHECK(key == a && value == 1);
    for (auto [key, value] : hash.asKeyValueRange())
        CHECK(key == b && value == 2);

    // A file that is created with its permissions: never readable by others, not even for a moment.
    const QString secret = dir + QStringLiteral("/secret.key");
    {
        QFile file(secret);
        CHECK(file.open(QIODevice::WriteOnly | QIODevice::NewOnly, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        CHECK(file.write("key") == 3);
        CHECK(!(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther |
                                      QFileDevice::WriteOther | QFileDevice::ExeOwner)));
        CHECK((file.permissions() & QFileDevice::ReadOwner) && (file.permissions() & QFileDevice::WriteOwner));
    }
    const QFileDevice::Permissions others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                            QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    CHECK(!(QFile::permissions(secret) & others));
    {
        // A file that exists keeps the permissions that it has.
        QFile file(secret);
        CHECK(file.open(QIODevice::WriteOnly | QIODevice::Append, QFileDevice::ReadOwner | QFileDevice::ReadOther));
        CHECK(!(file.permissions() & others) && (file.permissions() & QFileDevice::WriteOwner));
        CHECK(!file.open(QIODevice::ReadOnly, QFileDevice::ReadOwner)); // already open
    }
    {
        QFile readOnly(dir + QStringLiteral("/readonly.txt"));
        CHECK(readOnly.open(QIODevice::WriteOnly, QFileDevice::ReadOwner));
        CHECK(readOnly.write("once") == 4 && !(readOnly.permissions() & QFileDevice::WriteOwner));
        QFile nameless;
        CHECK(!nameless.open(QIODevice::WriteOnly, QFileDevice::ReadOwner) &&
              nameless.error() == QFileDevice::OpenError);
        QFile missing(dir + QStringLiteral("/no/such/dir/file"));
        CHECK(!missing.open(QIODevice::WriteOnly, QFileDevice::ReadOwner));
        QFile existing(dir + QStringLiteral("/nothing-here"));
        CHECK(!existing.open(QIODevice::ReadOnly | QIODevice::ExistingOnly, QFileDevice::ReadOwner));
    }
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
        QTemporaryDir dir;
        CHECK(dir.isValid());
        testBoolList();
        testQtCompatibility(dir.path());
    }
    CHECK(QCoreApplication::closingDown() && !QCoreApplication::instanceExists() && !QCoreApplication::startingUp());

    if (g_failures) {
        std::printf("%d more-core test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all more-core tests passed");
    return 0;
}
