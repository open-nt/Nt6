// MIT License - see LICENSE
#include <QtCore>

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static QString hexOf(const QByteArray &b) {
    QString r;
    for (qsizetype i = 0; i < b.size(); ++i) {
        char buf[8];
        std::snprintf(buf, sizeof buf, i ? " %02X" : "%02X", static_cast<unsigned>(static_cast<unsigned char>(b[i])));
        r += buf;
    }
    return r;
}

static QByteArray bytes(std::initializer_list<int> l) {
    QByteArray b;
    for (int v : l)
        b.append(static_cast<char>(v));
    return b;
}

// ---------------------------------------------------------------- QDataStream

static void testDataStreamIntegers() {
    QByteArray ba;
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << qint8(-2) << quint16(0x1234) << qint32(0x01020304) << qint64(0x0102030405060708LL) << true;
        CHECK(out.status() == QDataStream::Ok);
    }
    CHECK(hexOf(ba) == "FE 12 34 01 02 03 04 01 02 03 04 05 06 07 08 01");

    QDataStream in(ba);
    qint8 a = 0;
    quint16 b = 0;
    qint32 c = 0;
    qint64 d = 0;
    bool e = false;
    in >> a >> b >> c >> d >> e;
    CHECK(a == -2 && b == 0x1234 && c == 0x01020304 && d == 0x0102030405060708LL && e);
    CHECK(in.atEnd() && in.status() == QDataStream::Ok);

    QByteArray le;
    QDataStream lout(&le, QIODevice::WriteOnly);
    lout.setByteOrder(QDataStream::LittleEndian);
    CHECK(lout.byteOrder() == QDataStream::LittleEndian);
    lout << quint16(0x1234) << qint32(0x01020304) << quint64(0x0102030405060708ULL);
    CHECK(hexOf(le) == "34 12 04 03 02 01 08 07 06 05 04 03 02 01");
    QDataStream lin(le);
    lin.setByteOrder(QDataStream::LittleEndian);
    quint16 x = 0;
    qint32 y = 0;
    quint64 z = 0;
    lin >> x >> y >> z;
    CHECK(x == 0x1234 && y == 0x01020304 && z == 0x0102030405060708ULL);

    // Reading big-endian data with the wrong byte order swaps it.
    QDataStream wrong(ba);
    wrong.setByteOrder(QDataStream::LittleEndian);
    qint8 skip = 0;
    quint16 swapped = 0;
    wrong >> skip >> swapped;
    CHECK(swapped == 0x3412);

    QByteArray more;
    QDataStream mo(&more, QIODevice::WriteOnly);
    mo << 5LL << 7ULL << 'x' << static_cast<unsigned long>(9);
    CHECK(hexOf(more) == "00 00 00 00 00 00 00 05 00 00 00 00 00 00 00 07 78 00 00 00 00 00 00 00 09");
}

static void testDataStreamFloats() {
    QByteArray ba;
    QDataStream out(&ba, QIODevice::WriteOnly);
    CHECK(out.floatingPointPrecision() == QDataStream::DoublePrecision);
    out << 1.5 << 1.5f;
    CHECK(hexOf(ba) == "3F F8 00 00 00 00 00 00 3F F8 00 00 00 00 00 00");
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ba.clear();
    out.device()->seek(0);
    out << 1.5f << 1.5;
    CHECK(hexOf(ba) == "3F C0 00 00 3F C0 00 00");

    QDataStream in(ba);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);
    float f = 0;
    double d = 0;
    in >> f >> d;
    CHECK(f == 1.5f && d == 1.5);

    QByteArray old;
    QDataStream oo(&old, QIODevice::WriteOnly);
    oo.setVersion(QDataStream::Qt_4_5);
    CHECK(oo.version() == QDataStream::Qt_4_5);
    oo << 2.0f;
    CHECK(hexOf(old) == "40 00 00 00");
}

static void testDataStreamStrings() {
    QByteArray ba;
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << QString::fromUtf8("A\xC3\xA9");
    }
    CHECK(hexOf(ba) == "00 00 00 04 00 41 00 E9");

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << QString() << QString("");
    }
    CHECK(hexOf(ba) == "FF FF FF FF 00 00 00 00");
    {
        QDataStream in(ba);
        QString a("x"), b;
        in >> a >> b;
        CHECK(a.isNull() && !b.isNull() && b.isEmpty());
    }

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << QString(u"\U0001F600");
    }
    CHECK(hexOf(ba) == "00 00 00 04 D8 3D DE 00");

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out << QString::fromUtf8("A\xC3\xA9");
    }
    CHECK(hexOf(ba) == "04 00 00 00 41 00 E9 00");
    {
        QDataStream in(ba);
        in.setByteOrder(QDataStream::LittleEndian);
        QString s;
        in >> s;
        CHECK(s == QString::fromUtf8("A\xC3\xA9"));
    }

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << QChar(u'\u20AC') << QByteArray("abc") << QByteArray() << QByteArray("");
    }
    CHECK(hexOf(ba) == "20 AC 00 00 00 03 61 62 63 FF FF FF FF 00 00 00 00");
    {
        QDataStream in(ba);
        QChar c;
        QByteArray a, n("zz"), e;
        in >> c >> a >> n >> e;
        CHECK(c == QChar(u'\u20AC') && a == "abc" && n.isNull() && !e.isNull() && e.isEmpty());
        CHECK(in.atEnd());
    }

    // Data written by Qt: "Hi" as a QString.
    QDataStream fromQt(bytes({0, 0, 0, 4, 0, 0x48, 0, 0x69}));
    QString hi;
    fromQt >> hi;
    CHECK(hi == "Hi");
}

static void testDataStreamContainers() {
    QByteArray ba;
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        QList<qint32> l;
        l << 1 << 2;
        out << l;
    }
    CHECK(hexOf(ba) == "00 00 00 02 00 00 00 01 00 00 00 02");
    {
        QDataStream in(ba);
        QList<qint32> l;
        in >> l;
        CHECK(l.size() == 2 && l.at(0) == 1 && l.at(1) == 2 && in.status() == QDataStream::Ok);
    }

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << QStringList{QString("a")};
    }
    CHECK(hexOf(ba) == "00 00 00 01 00 00 00 02 00 61");

    ba.clear();
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        QMap<qint32, quint8> m;
        m.insert(3, 4);
        m.insert(1, 2);
        out << m;
        QHash<quint16, QString> h;
        h.insert(7, QString("z"));
        out << h;
    }
    CHECK(hexOf(ba) == "00 00 00 02 00 00 00 01 02 00 00 00 03 04 00 00 00 01 00 07 00 00 00 02 00 7A");
    {
        QDataStream in(ba);
        QMap<qint32, quint8> m;
        QHash<quint16, QString> h;
        in >> m >> h;
        CHECK(m.size() == 2 && m.value(1) == 2 && m.value(3) == 4);
        CHECK(h.size() == 1 && h.value(7) == "z");
    }

    // A truncated list reports ReadPastEnd and leaves the container empty.
    QDataStream trunc(bytes({0, 0, 0, 3, 0, 0, 0, 1}));
    QList<qint32> l;
    trunc >> l;
    CHECK(trunc.status() == QDataStream::ReadPastEnd && l.isEmpty());

    QList<QList<qint16>> nested;
    nested.append(QList<qint16>{1, 2});
    nested.append(QList<qint16>());
    QByteArray nb;
    QDataStream no(&nb, QIODevice::WriteOnly);
    no << nested;
    CHECK(hexOf(nb) == "00 00 00 02 00 00 00 02 00 01 00 02 00 00 00 00");
    QDataStream ni(nb);
    QList<QList<qint16>> back;
    ni >> back;
    CHECK(back == nested);
}

static void testDataStreamRaw() {
    QByteArray ba;
    QDataStream out(&ba, QIODevice::WriteOnly);
    CHECK(out.writeRawData("ab", 2) == 2);
    out.writeBytes("cd", 2);
    out << "hi";
    out << static_cast<const char *>(nullptr);
    CHECK(hexOf(ba) == "61 62 00 00 00 02 63 64 00 00 00 03 68 69 00 00 00 00 00");

    QDataStream in(ba);
    char raw[2];
    CHECK(in.readRawData(raw, 2) == 2 && raw[0] == 'a' && raw[1] == 'b');
    char *s = nullptr;
    qint64 len = 0;
    in.readBytes(s, len);
    CHECK(s && len == 2 && std::strcmp(s, "cd") == 0);
    delete[] s;
    char *cs = nullptr;
    in >> cs;
    CHECK(cs && std::strcmp(cs, "hi") == 0);
    delete[] cs;
    in.readBytes(s, len);
    CHECK(s == nullptr && len == 0 && in.status() == QDataStream::Ok && in.atEnd());

    QDataStream sk(QByteArray("0123456789"));
    CHECK(sk.skipRawData(3) == 3);
    char buf[4];
    CHECK(sk.readRawData(buf, 4) == 4 && std::memcmp(buf, "3456", 4) == 0);
    CHECK(sk.skipRawData(100) == 3 && sk.status() == QDataStream::ReadPastEnd);
}

static void testDataStreamStatus() {
    QDataStream in(bytes({0, 1}));
    qint32 v = 99;
    in >> v;
    CHECK(v == 0 && in.status() == QDataStream::ReadPastEnd);
    in.setStatus(QDataStream::ReadCorruptData);
    CHECK(in.status() == QDataStream::ReadPastEnd);
    in.resetStatus();
    CHECK(in.status() == QDataStream::Ok);
    in.setStatus(QDataStream::ReadCorruptData);
    CHECK(in.status() == QDataStream::ReadCorruptData);

    QDataStream odd(bytes({0, 0, 0, 3, 0, 65, 0}));
    QString s;
    odd >> s;
    CHECK(odd.status() == QDataStream::ReadCorruptData && s.isNull());

    QDataStream shortStr(bytes({0, 0, 0, 4, 0, 65}));
    shortStr >> s;
    CHECK(shortStr.status() == QDataStream::ReadPastEnd && s.isNull());

    QDataStream hugeLen(bytes({0x7F, 0xFF, 0xFF, 0xFF, 1, 2}));
    QByteArray ba;
    hugeLen >> ba;
    CHECK(hugeLen.status() == QDataStream::ReadPastEnd && ba.isEmpty());

    // A read-only buffer rejects writes.
    QByteArray fixed("abcd");
    QDataStream ro(&fixed, QIODevice::ReadOnly);
    ro << qint32(1);
    CHECK(ro.status() == QDataStream::WriteFailed);

    QDataStream none;
    CHECK(none.device() == nullptr && none.atEnd());
    qint32 q = 5;
    none >> q;
    CHECK(q == 0);

    QBuffer dev;
    dev.open(QIODevice::ReadWrite);
    QDataStream onDev(&dev);
    CHECK(onDev.device() == &dev);
    onDev << qint16(0x0A0B);
    dev.seek(0);
    qint16 r = 0;
    onDev >> r;
    CHECK(r == 0x0A0B && onDev.atEnd());
    QDataStream other;
    other.setDevice(&dev);
    CHECK(other.device() == &dev);
}

enum class Color : quint8 { Red = 1, Green = 2 };

static void testDataStreamEnum() {
    QByteArray ba;
    QDataStream out(&ba, QIODevice::WriteOnly);
    out << Color::Green;
    CHECK(hexOf(ba) == "02");
    QDataStream in(ba);
    Color c = Color::Red;
    in >> c;
    CHECK(c == Color::Green);
}

// ---------------------------------------------------------------- QTextStream

static void testTextStreamWriteBasics() {
    QString s;
    QTextStream ts(&s);
    ts << "a" << 1 << ' ' << 2.5 << QString(" x") << QChar('y') << Qt::endl;
    CHECK(s == "a1 2.5 xy\n");
    CHECK(ts.status() == QTextStream::Ok);

    s.clear();
    ts << -5 << ' ' << 7u << ' ' << 123456789012LL << ' ' << 18446744073709551615ULL << ' ' << short(-3);
    CHECK(s == "-5 7 123456789012 18446744073709551615 -3");

    s.clear();
    ts << QLatin1StringView("lat") << QByteArray("é") << QStringView(u"v") << true;
    CHECK(s == "latév1");

    s.clear();
    int dummy = 0;
    ts << static_cast<const void *>(&dummy);
    CHECK(s.startsWith("0x") && s.size() > 2);
}

static void testTextStreamFormatting() {
    QString s;
    QTextStream ts(&s);

    ts << qSetFieldWidth(5) << 42 << qSetFieldWidth(0);
    CHECK(s == "   42");
    s.clear();
    ts << Qt::left << qSetFieldWidth(5) << qSetPadChar('*') << 42 << "ab" << qSetFieldWidth(0);
    CHECK(s == "42***ab***");
    s.clear();
    ts << Qt::center << qSetFieldWidth(6) << "ab" << qSetFieldWidth(5) << "ab" << qSetFieldWidth(0);
    CHECK(s == "**ab**" "*ab**");
    ts.reset();
    CHECK(ts.fieldWidth() == 0 && ts.padChar() == QChar(' ') && ts.fieldAlignment() == QTextStream::AlignRight);

    s.clear();
    ts << qSetFieldWidth(6) << qSetPadChar('0') << -42 << qSetFieldWidth(0);
    CHECK(s == "000-42");
    s.clear();
    ts.setFieldAlignment(QTextStream::AlignAccountingStyle);
    ts << qSetFieldWidth(6) << -42 << qSetFieldWidth(0);
    CHECK(s == "-00042");
    ts.reset();
    s.clear();

    ts << Qt::hex << 255 << ' ' << Qt::showbase << 255 << ' ' << Qt::uppercasedigits << 255 << ' '
       << Qt::uppercasebase << 255 << ' ' << -255 << ' ' << Qt::lowercasedigits << Qt::lowercasebase
       << Qt::noshowbase << 0xabc;
    CHECK(s == "ff 0xff 0xFF 0XFF -0XFF abc");
    s.clear();
    ts.reset();
    ts << Qt::bin << 5 << ' ' << Qt::showbase << 5 << ' ' << Qt::oct << 8 << ' ' << Qt::dec << Qt::noshowbase << 8
       << ' ' << Qt::forcesign << 5 << ' ' << -5 << ' ' << 0 << Qt::noforcesign;
    CHECK(s == "101 0b101 010 8 +5 -5 +0");
    CHECK(ts.integerBase() == 10);
    s.clear();
    ts.reset();
    ts.setIntegerBase(36);
    ts << 35;
    CHECK(s == "z");

    s.clear();
    ts.reset();
    ts << 3.14159265 << ' ' << 1.0 << ' ' << 100000.0 << ' ' << 1e6 << ' ' << 0.5f << ' ' << 1e-5;
    CHECK(s == "3.14159 1 100000 1e+06 0.5 1e-05");
    s.clear();
    ts << Qt::fixed << qSetRealNumberPrecision(2) << 3.14159 << ' ' << -0.004 << ' ' << 2.0;
    CHECK(s == "3.14 -0.00 2.00");
    CHECK(ts.realNumberNotation() == QTextStream::FixedNotation && ts.realNumberPrecision() == 2);
    s.clear();
    ts << Qt::scientific << qSetRealNumberPrecision(3) << 12345.678;
    CHECK(s == "1.235e+04");
    s.clear();
    ts.reset();
    ts << Qt::fixed << qSetRealNumberPrecision(0) << Qt::forcepoint << 3.0 << ' ' << Qt::noforcepoint << 3.0;
    CHECK(s == "3. 3");
    s.clear();
    ts.reset();
    ts << Qt::forcesign << 1.5 << ' ' << -1.5 << Qt::noforcesign << ' ' << Qt::scientific << qSetRealNumberPrecision(2)
       << Qt::uppercasebase << 1234.5;
    CHECK(s == "+1.5 -1.5 1.23E+03");
    s.clear();
    ts.reset();
    ts << qSetFieldWidth(8) << 2.5 << qSetFieldWidth(0) << ' ' << std::numeric_limits<double>::infinity() << ' '
       << -std::numeric_limits<double>::infinity() << ' ' << std::numeric_limits<double>::quiet_NaN();
    CHECK(s == "     2.5 inf -inf nan");
}

static void testTextStreamReading() {
    QString src = QString("  12 -7 3.5 hello\nline2\r\nlast");
    QTextStream ts(&src, QIODevice::ReadOnly);
    int a = 0, b = 0;
    double d = 0;
    QString w;
    ts >> a >> b >> d >> w;
    CHECK(a == 12 && b == -7 && d == 3.5 && w == "hello");
    CHECK(ts.status() == QTextStream::Ok);
    CHECK(ts.readLine().isEmpty());
    CHECK(ts.readLine() == "line2");
    CHECK(!ts.atEnd());
    CHECK(ts.readLine() == "last");
    CHECK(ts.atEnd());
    QString none = ts.readLine();
    CHECK(none.isNull());
    int extra = 1;
    ts >> extra;
    CHECK(extra == 0 && ts.status() == QTextStream::ReadPastEnd);
    ts.resetStatus();
    CHECK(ts.status() == QTextStream::Ok);

    QString lines = QString("abcdef\nxyz\n\nend");
    QTextStream ls(&lines);
    CHECK(ls.readLine(3) == "abc");
    CHECK(ls.readLine() == "def");
    QString into;
    CHECK(ls.readLineInto(&into) && into == "xyz");
    CHECK(ls.readLineInto(&into) && into.isEmpty() && !into.isNull());
    CHECK(ls.readAll() == "end");
    CHECK(ls.readAll().isEmpty() && !ls.readLineInto(&into));

    QString chars = QString("  x  yz tail");
    QTextStream cs(&chars);
    QChar c1, c2;
    char c3 = 0;
    cs >> c1 >> c2 >> c3;
    CHECK(c1 == QChar('x') && c2 == QChar('y') && c3 == 'z');
    cs >> Qt::ws;
    CHECK(cs.pos() == 8);
    CHECK(cs.read(2) == "ta");
    CHECK(cs.read(100) == "il");
    CHECK(cs.read(5).isEmpty() && cs.atEnd());
    CHECK(cs.seek(2) && cs.read(1) == "x");
    CHECK(!cs.seek(1000));
    cs.skipWhiteSpace();
    CHECK(cs.pos() == 5);

    QString nums = QString("0x1f 010 0b11 7 ff -ff 12abc 99999 -1 3e2 1.5e-1x nan");
    QTextStream ns(&nums);
    ns.setIntegerBase(0);
    int i1, i2, i3, i4;
    ns >> i1 >> i2 >> i3 >> i4;
    CHECK(i1 == 31 && i2 == 8 && i3 == 3 && i4 == 7);
    ns.setIntegerBase(16);
    ns >> i1 >> i2;
    CHECK(i1 == 255 && i2 == -255);
    ns.setIntegerBase(10);
    QString rest;
    ns >> i1 >> rest;
    CHECK(i1 == 12 && rest == "abc");
    short sh = 5;
    ns >> sh;
    CHECK(sh == 0 && ns.status() == QTextStream::ReadCorruptData);
    ns.resetStatus();
    unsigned int u = 5;
    ns >> u;
    CHECK(u == 0 && ns.status() == QTextStream::ReadCorruptData);
    ns.resetStatus();
    double e1 = 0, e2 = 0, e3 = 0;
    ns >> e1 >> e2;
    CHECK(e1 == 300.0 && e2 == 0.15);
    ns >> rest >> e3;
    CHECK(rest == "x" && e3 != e3 && ns.status() == QTextStream::Ok);

    QString bad = QString("abc");
    QTextStream bs(&bad);
    int bi = 7;
    bs >> bi;
    CHECK(bi == 0 && bs.status() == QTextStream::ReadCorruptData);
    CHECK(bs.pos() == 0);
}

static void testTextStreamTargets() {
    QByteArray out;
    {
        QTextStream ts(&out);
        ts << QString::fromUtf8("h\xC3\xA9") << " " << 5;
        ts.flush();
        CHECK(hexOf(out) == "68 C3 A9 20 35");
    }
    out.clear();
    {
        QTextStream ts(&out, QIODevice::WriteOnly);
        ts << QString(u"\U0001F600");
    }
    CHECK(hexOf(out) == "F0 9F 98 80");

    QTextStream in(QByteArray("caf\xC3\xA9 \xE2\x82\xAC\nline"));
    QString w1, w2;
    in >> w1 >> w2;
    CHECK(w1 == QString::fromUtf8("caf\xC3\xA9") && w2 == QString::fromUtf8("\xE2\x82\xAC"));
    CHECK(in.pos() == 9);
    CHECK(in.readLine().isEmpty());
    CHECK(in.readLine() == "line" && in.atEnd());
    CHECK(in.seek(1) && in.pos() == 1 && in.read(2) == QString::fromUtf8("af") && in.pos() == 3);

    // A multi-byte sequence straddling the internal read chunk must survive.
    QByteArray big(4095, 'a');
    big.append("\xC3\xA9");
    big.append("tail");
    QTextStream bigs(big);
    const QString all = bigs.readAll();
    CHECK(all.size() == 4095 + 1 + 4 && all.at(4095) == QChar(0xE9) && all.endsWith("tail"));

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);
    QTextStream rw(&buf);
    CHECK(rw.device() == &buf);
    rw << "hello world\nsecond";
    rw.flush();
    CHECK(buf.data() == "hello world\nsecond");
    CHECK(rw.pos() == 18);
    rw.seek(0);
    CHECK(rw.readLine() == "hello world");
    rw.seek(6);
    rw << "WORLD";
    rw.flush();
    CHECK(buf.data() == "hello WORLD\nsecond");
    rw.seek(0);
    CHECK(rw.readAll() == "hello WORLD\nsecond");

    QString target = "pre:";
    QTextStream appender(&target);
    appender << 42;
    CHECK(target == "pre:42");
    appender.setString(nullptr);
    CHECK(appender.string() == nullptr);

    QTextStream detached;
    detached << "ignored" << 1;
    CHECK(detached.atEnd() && detached.readLine().isNull());

    // Writing to a device that is not writable reports an error.
    QBuffer ro;
    ro.setData(QByteArray("x"));
    ro.open(QIODevice::ReadOnly);
    QTextStream bad(&ro);
    bad << "no";
    bad.flush();
    CHECK(bad.status() == QTextStream::WriteFailed);
}

static void testTextStreamFile() {
    const char *path = "nt_streams_test_tmp.txt";
    FILE *f = std::fopen(path, "w+");
    CHECK(f != nullptr);
    if (!f)
        return;
    {
        QTextStream ts(f);
        ts << "one " << 2 << Qt::endl << "three";
        ts.flush();
        CHECK(ts.pos() == 11);
        ts.seek(0);
        CHECK(ts.readLine() == "one 2");
        CHECK(ts.readAll() == "three");
    }
    std::fclose(f);

    QFile file(QString::fromUtf8(path));
    CHECK(file.open(QIODevice::ReadOnly));
    QTextStream fs(&file);
    CHECK(fs.readLine() == "one 2" && fs.readLine() == "three" && fs.atEnd());
    file.close();
    QFile::remove(QString::fromUtf8(path));

    QFile out(QString::fromUtf8(path));
    CHECK(out.open(QIODevice::WriteOnly));
    {
        QTextStream fo(&out);
        fo << QString::fromUtf8("\xC3\xA9") << 1;
    }
    out.close();
    CHECK(out.open(QIODevice::ReadOnly));
    CHECK(out.readAll() == "\xC3\xA9" "1");
    out.close();
    QFile::remove(QString::fromUtf8(path));
}

// ---------------------------------------------------------- QCommandLineParser

static QCommandLineParser *makeParser(QCommandLineOption *outOpt = nullptr) {
    auto *p = new QCommandLineParser;
    p->setApplicationDescription("Test helper");
    p->addHelpOption();
    p->addVersionOption();
    p->addOption(QCommandLineOption("a", "Flag a."));
    p->addOption(QCommandLineOption("b", "Flag b."));
    p->addOption(QCommandLineOption("c", "Flag c."));
    p->addOption(QCommandLineOption(QStringList{"V", "verbose"}, "Be chatty."));
    QCommandLineOption out(QStringList{"o", "out"}, "Output file.", "file", "a.out");
    p->addOption(out);
    p->addOption(QCommandLineOption(QStringList{"I", "include"}, "Include dir.", "dir"));
    if (outOpt)
        *outOpt = out;
    p->addPositionalArgument("source", "Source file.");
    p->addPositionalArgument("destination", "Destination dir.", "[destination]");
    return p;
}

static void testParserBasics() {
    QCommandLineOption outOpt("x");
    std::unique_ptr<QCommandLineParser> p(makeParser(&outOpt));

    CHECK(p->parse(QStringList{"prog"}));
    CHECK(!p->isSet("a") && !p->isSet("out") && p->positionalArguments().isEmpty());
    CHECK(p->value("out") == "a.out" && p->values("out") == QStringList{"a.out"});
    CHECK(p->value("include").isEmpty() && p->values("include").isEmpty());
    CHECK(p->errorText().isEmpty() && p->optionNames().isEmpty());

    CHECK(p->parse(QStringList{"prog", "-V", "--verbose", "file1", "file2"}));
    CHECK(p->isSet("V") && p->isSet("verbose") && p->isSet("a") == false);
    CHECK(p->optionNames() == (QStringList{"V", "verbose"}));
    CHECK(p->positionalArguments() == (QStringList{"file1", "file2"}));

    CHECK(p->parse(QStringList{"prog", "-abc"}));
    CHECK(p->isSet("a") && p->isSet("b") && p->isSet("c"));
    CHECK(p->optionNames() == (QStringList{"a", "b", "c"}));

    CHECK(p->parse(QStringList{"prog", "-ofile"}));
    CHECK(p->isSet("out") && p->isSet("o") && p->value("out") == "file" && p->value(outOpt) == "file" && p->isSet(outOpt));
    CHECK(p->parse(QStringList{"prog", "-o", "file"}) && p->value("o") == "file");
    CHECK(p->parse(QStringList{"prog", "--out=file"}) && p->value("out") == "file");
    CHECK(p->parse(QStringList{"prog", "--out", "file"}) && p->value("out") == "file");
    CHECK(p->parse(QStringList{"prog", "--out="}) && p->isSet("out") && p->value("out").isEmpty());
    CHECK(p->parse(QStringList{"prog", "-o=eq"}) && p->value("out") == "eq");
    CHECK(p->parse(QStringList{"prog", "--out=a=b"}) && p->value("out") == "a=b");

    CHECK(p->parse(QStringList{"prog", "-abofile", "-V"}));
    CHECK(p->isSet("a") && p->isSet("b") && !p->isSet("c") && p->value("out") == "file" && p->isSet("verbose"));

    CHECK(p->parse(QStringList{"prog", "-I", "x", "--include=y", "-Iz"}));
    CHECK(p->values("include") == (QStringList{"x", "y", "z"}) && p->value("include") == "z");

    CHECK(p->parse(QStringList{"prog", "-o", "one", "-o", "two"}));
    CHECK(p->value("out") == "two" && p->values("out").size() == 2);

    // Values are taken verbatim, even when they look like options.
    CHECK(p->parse(QStringList{"prog", "-o", "--verbose"}));
    CHECK(p->value("out") == "--verbose" && !p->isSet("verbose"));

    // "--" ends option parsing; a lone "-" is positional.
    CHECK(p->parse(QStringList{"prog", "-a", "--", "-b", "--out", "-"}));
    CHECK(p->isSet("a") && !p->isSet("b") && !p->isSet("out"));
    CHECK(p->positionalArguments() == (QStringList{"-b", "--out", "-"}));
    CHECK(p->parse(QStringList{"prog", "-", "x"}) && p->positionalArguments() == (QStringList{"-", "x"}));

    // Options may follow positional arguments.
    CHECK(p->parse(QStringList{"prog", "src", "-a", "dst"}));
    CHECK(p->isSet("a") && p->positionalArguments() == (QStringList{"src", "dst"}));
    p->setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    CHECK(p->parse(QStringList{"prog", "-b", "src", "-a", "--out"}));
    CHECK(p->isSet("b") && !p->isSet("a") && p->positionalArguments() == (QStringList{"src", "-a", "--out"}));
    p->setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    // The built-in options are recognised, process() acts on them.
    CHECK(p->parse(QStringList{"prog", "--help"}) && p->isSet("help") && p->isSet("h"));
    CHECK(p->parse(QStringList{"prog", "-v"}) && p->isSet("version"));
}

static void testParserErrors() {
    std::unique_ptr<QCommandLineParser> p(makeParser());

    CHECK(!p->parse(QStringList{"prog", "--nope", "pos"}));
    CHECK(p->unknownOptionNames() == QStringList{"nope"});
    CHECK(p->errorText() == "Unknown option 'nope'.");
    CHECK(p->positionalArguments() == QStringList{"pos"});

    CHECK(!p->parse(QStringList{"prog", "-z", "--bad", "-a"}));
    CHECK(p->unknownOptionNames() == (QStringList{"z", "bad"}));
    CHECK(p->errorText() == "Unknown options: z, bad.");
    CHECK(p->isSet("a"));

    CHECK(!p->parse(QStringList{"prog", "--out"}));
    CHECK(p->errorText() == "Missing value after '--out'.");
    CHECK(!p->parse(QStringList{"prog", "-o"}));
    CHECK(p->errorText() == "Missing value after '-o'.");
    CHECK(!p->parse(QStringList{"prog", "--verbose=1"}));
    CHECK(p->errorText() == "Unexpected value after '--verbose'.");

    // A successful parse clears earlier state.
    CHECK(p->parse(QStringList{"prog", "x"}));
    CHECK(p->errorText().isEmpty() && p->unknownOptionNames().isEmpty() && !p->isSet("a"));

    QCommandLineParser dup;
    CHECK(dup.addOption(QCommandLineOption(QStringList{"x", "xx"})));
    CHECK(!dup.addOption(QCommandLineOption("xx")));
    CHECK(!dup.addOption(QCommandLineOption(QStringList{"y", "x"})));
    CHECK(!dup.addOption(QCommandLineOption(QStringList{"-bad", "="})));
    CHECK(dup.addOptions(QList<QCommandLineOption>{QCommandLineOption("p"), QCommandLineOption("q")}));
    CHECK(!dup.addOptions(QList<QCommandLineOption>{QCommandLineOption("r"), QCommandLineOption("p")}));
    CHECK(QCommandLineOption(QStringList{"ok", "-no", "a=b", ""}).names() == QStringList{"ok"});
}

static void testParserDashModes() {
    QCommandLineParser p;
    p.addOption(QCommandLineOption("a"));
    p.addOption(QCommandLineOption("b"));
    p.addOption(QCommandLineOption(QStringList{"long"}, "Long.", "v"));
    p.addOption(QCommandLineOption("flag"));

    CHECK(!p.parse(QStringList{"prog", "-long", "x"}));
    p.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    CHECK(p.parse(QStringList{"prog", "-long", "x", "-flag", "-a", "-long=y"}));
    CHECK(p.values("long") == (QStringList{"x", "y"}) && p.isSet("flag") && p.isSet("a"));
    CHECK(!p.parse(QStringList{"prog", "-ab"}));
    CHECK(p.unknownOptionNames() == QStringList{"ab"});
    p.setSingleDashWordOptionMode(QCommandLineParser::ParseAsCompactedShortOptions);
    CHECK(p.parse(QStringList{"prog", "-ab"}) && p.isSet("a") && p.isSet("b"));
}

static void testParserHelp() {
    std::unique_ptr<QCommandLineParser> p(makeParser());
    CHECK(p->parse(QStringList{"prog"}));
    const QString expected =
        "Usage: prog [options] source [destination]\n"
        "Test helper\n"
        "\n"
        "Options:\n"
        "  -h, --help           Displays help on commandline options.\n"
        "  -v, --version        Displays version information.\n"
        "  -a                   Flag a.\n"
        "  -b                   Flag b.\n"
        "  -c                   Flag c.\n"
        "  -V, --verbose        Be chatty.\n"
        "  -o, --out <file>     Output file.\n"
        "  -I, --include <dir>  Include dir.\n"
        "\n"
        "Arguments:\n"
        "  source       Source file.\n"
        "  destination  Destination dir.\n";
    CHECK(p->helpText() == expected);
    if (p->helpText() != expected)
        std::printf("--- got ---\n%s", p->helpText().toUtf8().constData());

    QCommandLineParser q;
    q.addOption(QCommandLineOption("n"));
    QCommandLineOption hidden("secret", "Hidden.");
    hidden.setFlags(QCommandLineOption::HiddenFromHelp);
    q.addOption(hidden);
    QCommandLineOption style(QStringList{"single"}, "Single dash.");
    style.setFlags(QCommandLineOption::ShortOptionStyle);
    q.addOption(style);
    q.addOption(QCommandLineOption(QStringList{"w"}, "This description is deliberately long so that it has to be wrapped onto "
                                                       "a second line when the help text is laid out.", "n"));
    CHECK(q.parse(QStringList{"./tool"}));
    const QString helpQ = q.helpText();
    CHECK(helpQ.startsWith("Usage: ./tool [options]\n\nOptions:\n"));
    CHECK(!helpQ.contains("secret") && !helpQ.contains("Arguments:") && helpQ.contains("  -single"));
    const QStringList lines = helpQ.split('\n');
    bool wrapped = false;
    for (qsizetype i = 0; i + 1 < lines.size(); ++i)
        if (lines.at(i).contains("-w <n>")) {
            CHECK(lines.at(i).size() <= 79);
            CHECK(lines.at(i + 1).startsWith(QString(11, QChar(' '))) && lines.at(i + 1).trimmed().size() > 0);
            wrapped = true;
        }
    CHECK(wrapped);

    QCommandLineParser bare;
    CHECK(bare.parse(QStringList{"prog"}));
    CHECK(bare.helpText() == "Usage: prog\n\n");
}

static void testApplicationInfo() {
    CHECK(QCoreApplication::applicationName() == "nt_streams_tests" || !QCoreApplication::applicationName().isEmpty());
    QCoreApplication::setApplicationName("Demo");
    QCoreApplication::setApplicationVersion("1.2.3");
    CHECK(QCoreApplication::applicationName() == "Demo" && QCoreApplication::applicationVersion() == "1.2.3");
    CHECK(!QCoreApplication::arguments().isEmpty());
}

static QString runProcess(const QStringList &args, int *exitCode) {
    int fds[2];
    if (pipe(fds) != 0)
        return QString();
    const pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        QCommandLineParser p;
        p.setApplicationDescription("Desc");
        p.addHelpOption();
        p.addVersionOption();
        p.addOption(QCommandLineOption("o", "Out.", "file"));
        p.process(args);
        std::fputs("returned", stdout);
        std::fflush(stdout);
        _exit(0);
    }
    close(fds[1]);
    QByteArray out;
    char buf[256];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0)
        out.append(buf, n);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    *exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return QString::fromUtf8(out);
}

static void testParserProcess() {
    int code = -1;
    QString out = runProcess(QStringList{"prog", "--help"}, &code);
    CHECK(code == 0 && out.startsWith("Usage: prog [options]\nDesc\n\nOptions:\n"));
    out = runProcess(QStringList{"prog", "-v"}, &code);
    CHECK(code == 0 && out == "Demo 1.2.3\n");
    out = runProcess(QStringList{"prog", "--bogus"}, &code);
    CHECK(code == 1 && out == "Demo: Unknown option 'bogus'.\n");
    out = runProcess(QStringList{"prog", "-o", "f", "x"}, &code);
    CHECK(code == 0 && out == "returned");
}

// ------------------------------------------------- QIODevice transactions

// A sequential device whose data arrives in pieces, like a socket.
class ChunkDevice : public QIODevice {
public:
    void feed(const QByteArray &d) { pending.append(d); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return QIODevice::bytesAvailable() + pending.size(); }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        const qint64 n = std::min<qint64>(maxSize, pending.size());
        std::memcpy(data, pending.constData(), static_cast<std::size_t>(n));
        pending.remove(0, n);
        return n;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QByteArray pending;
};

static void testDeviceTransactions() {
    {
        ChunkDevice dev;
        dev.open(QIODevice::ReadOnly);
        dev.feed("abcdef");
        dev.startTransaction();
        CHECK(dev.isTransactionStarted());
        CHECK(dev.read(2) == "ab");
        CHECK(dev.peek(1) == "c");
        CHECK(dev.read(3) == "cde");
        dev.rollbackTransaction();
        CHECK(!dev.isTransactionStarted() && dev.bytesAvailable() == 6);
        CHECK(dev.read(4) == "abcd");
        dev.startTransaction();
        dev.feed("gh\nij");
        CHECK(dev.readLine() == "efgh\n");
        dev.commitTransaction();
        CHECK(dev.readAll() == "ij");
        dev.feed("0123456789");
        CHECK(dev.skip(3) == 3 && dev.read(2) == "34");
        dev.startTransaction();
        CHECK(dev.skip(100) == 5 && dev.atEnd());
        dev.rollbackTransaction();
        CHECK(dev.readAll() == "56789");
        dev.feed("xy");
        char c = 0;
        dev.startTransaction();
        CHECK(dev.getChar(&c) && c == 'x');
        dev.ungetChar(c);
        CHECK(dev.read(2) == "xy");
        dev.rollbackTransaction();
        CHECK(dev.read(2) == "xy");
    }
    {
        QBuffer buf;
        buf.setData("line one\nline two");
        buf.open(QIODevice::ReadOnly);
        buf.startTransaction();
        CHECK(buf.readLine() == "line one\n");
        buf.rollbackTransaction();
        CHECK(buf.pos() == 0);
        QByteArray line;
        CHECK(buf.readLineInto(&line) && line == "line one\n");
        CHECK(buf.readLineInto(&line) && line == "line two");
        CHECK(!buf.readLineInto(&line) && line.isEmpty());
        CHECK(buf.seek(2) && buf.skip(3) == 3 && buf.pos() == 5 && buf.skip(1000) == 12 && buf.atEnd());
        CHECK(buf.skip(4) == 0);
        buf.close();
        CHECK(buf.skip(1) == -1);
    }
    {
        // QDataStream on a device that receives a message in two parts.
        QByteArray message;
        {
            QDataStream out(&message, QIODevice::WriteOnly);
            out << QString("hello") << qint32(42);
        }
        ChunkDevice dev;
        dev.open(QIODevice::ReadOnly);
        QDataStream in(&dev);
        dev.feed(message.left(7));
        QString text;
        qint32 number = 0;
        in.startTransaction();
        CHECK(in.isDeviceTransactionStarted());
        in >> text >> number;
        CHECK(!in.commitTransaction() && !in.isDeviceTransactionStarted());
        CHECK(dev.bytesAvailable() == 7);
        dev.feed(message.mid(7));
        in.startTransaction();
        in.startTransaction(); // nested: only the outermost acts on the device
        in >> text;
        CHECK(in.commitTransaction() && in.isDeviceTransactionStarted());
        in >> number;
        CHECK(in.commitTransaction() && text == "hello" && number == 42 && in.status() == QDataStream::Ok);
        dev.feed(bytes({1, 2, 3}));
        in.startTransaction();
        qint8 b = 0;
        in >> b;
        in.rollbackTransaction();
        CHECK(in.status() == QDataStream::ReadPastEnd && dev.bytesAvailable() == 3);
        in.startTransaction();
        in >> b;
        in.abortTransaction();
        CHECK(in.status() == QDataStream::ReadCorruptData && dev.bytesAvailable() == 2);
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Demo");
    QCoreApplication::setApplicationVersion("1.2.3");
    testDataStreamIntegers();
    testDataStreamFloats();
    testDataStreamStrings();
    testDataStreamContainers();
    testDataStreamRaw();
    testDataStreamStatus();
    testDataStreamEnum();
    testDeviceTransactions();
    testTextStreamWriteBasics();
    testTextStreamFormatting();
    testTextStreamReading();
    testTextStreamTargets();
    testTextStreamFile();
    testParserBasics();
    testParserErrors();
    testParserDashModes();
    testParserHelp();
    testParserProcess();
    testApplicationInfo();
    if (g_failures == 0)
        std::puts("all stream tests passed");
    return g_failures == 0 ? 0 : 1;
}
