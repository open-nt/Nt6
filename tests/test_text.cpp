// MIT License - see LICENSE
#include <QByteArray>
#include <QString>
#include <QStringConverter>
#include <QAnyStringView>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QStringList>
#include <QStringMatcher>
#include <QStringTokenizer>
#include <QUtf8StringView>

#include <cstdio>
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

using E = QStringConverter;

static QByteArray bytes(std::initializer_list<int> v) {
    QByteArray b;
    for (int x : v)
        b.append(static_cast<char>(x));
    return b;
}

static QString str(std::initializer_list<int> v) {
    QString s;
    for (int x : v)
        s.append(QChar(static_cast<char16_t>(x)));
    return s;
}

static void testNames() {
    CHECK(E::encodingForName("UTF-8") == E::Utf8);
    CHECK(E::encodingForName("utf8") == E::Utf8);
    CHECK(E::encodingForName("utf_8") == E::Utf8);
    CHECK(E::encodingForName("UTF-16LE") == E::Utf16LE);
    CHECK(E::encodingForName("utf-16be") == E::Utf16BE);
    CHECK(E::encodingForName("UTF-32") == E::Utf32);
    CHECK(E::encodingForName("Latin1") == E::Latin1);
    CHECK(E::encodingForName("ISO-8859-1") == E::Latin1);
    CHECK(E::encodingForName("System") == E::System);
    CHECK(E::encodingForName("UTF-8xyz", 5) == E::Utf8);
    CHECK(!E::encodingForName("windows-1252").has_value());
    CHECK(!E::encodingForName("nonsense").has_value());
    CHECK(!E::encodingForName(nullptr).has_value());
    CHECK(std::string(E::nameForEncoding(E::Utf8)) == "UTF-8");
    CHECK(std::string(E::nameForEncoding(E::Utf16)) == "UTF-16");
    CHECK(std::string(E::nameForEncoding(E::Utf32BE)) == "UTF-32BE");
    CHECK(std::string(E::nameForEncoding(E::Latin1)) == "ISO-8859-1");
    CHECK(std::string(E::nameForEncoding(E::System)) == "Locale");

    QStringDecoder none;
    CHECK(!none.isValid());
    CHECK(none.name() == nullptr);
    CHECK(QString(none("abc")).isEmpty());
    QStringDecoder bad("no-such-encoding");
    CHECK(!bad.isValid());
    QStringEncoder good("windows-1252");
    CHECK(good.isValid());
    CHECK(std::string(good.name()) == "windows-1252");
    CHECK(std::string(QStringEncoder(E::Utf8).name()) == "UTF-8");
    CHECK(QStringEncoder("cp1252").isValid());
    CHECK(QStringEncoder("Latin-2").isValid());
    CHECK(std::string(QStringEncoder("Latin-2").name()) == "ISO-8859-2");
    CHECK(QStringEncoder("koi8-r").isValid());
    CHECK(QStringEncoder("US-ASCII").isValid());
    CHECK(QStringEncoder("latin1").isValid());
}

static void testUtf8() {
    QStringDecoder dec(E::Utf8);
    const QString all = dec("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
    CHECK(all == str({'a', 0xe9, 0x20ac, 0xd83d, 0xde00}));
    CHECK(!dec.hasError());

    QStringEncoder enc(E::Utf8);
    CHECK(QByteArray(enc(all)) == QByteArray("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"));
    CHECK(!enc.hasError());

    // The BOM is skipped unless ConvertInitialBom is set.
    CHECK(QString(QStringDecoder(E::Utf8)("\xef\xbb\xbf" "x")) == QString("x"));
    CHECK(QString(QStringDecoder(E::Utf8, QStringConverter::Flag::ConvertInitialBom)("\xef\xbb\xbf" "x")) == str({0xfeff, 'x'}));
    CHECK(QByteArray(QStringEncoder(E::Utf8, QStringConverter::Flag::WriteBom)(u"x")) == QByteArray("\xef\xbb\xbf" "x"));

    // BOM is written only once for a stateful encoder.
    QStringEncoder bomEnc(E::Utf8, QStringConverter::Flag::WriteBom);
    CHECK(QByteArray(bomEnc(u"a")) == QByteArray("\xef\xbb\xbf" "a"));
    CHECK(QByteArray(bomEnc(u"b")) == QByteArray("b"));
    QStringEncoder bomEnc2(E::Utf8, QStringConverter::Flag::WriteBom | QStringConverter::Flag::Stateless);
    CHECK(QByteArray(bomEnc2(u"a")) == QByteArray("\xef\xbb\xbf" "a"));
    CHECK(QByteArray(bomEnc2(u"b")) == QByteArray("\xef\xbb\xbf" "b"));

    // Chunk boundaries inside a sequence.
    const QByteArray src("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z");
    for (qsizetype cut1 = 0; cut1 <= src.size(); ++cut1) {
        for (qsizetype cut2 = cut1; cut2 <= src.size(); ++cut2) {
            QStringDecoder d(E::Utf8);
            QString r;
            r += d(QByteArrayView(src.constData(), cut1));
            r += d(QByteArrayView(src.constData() + cut1, cut2 - cut1));
            r += d(QByteArrayView(src.constData() + cut2, src.size() - cut2));
            CHECK(r == QString::fromUtf8(src));
            CHECK(!d.hasError());
        }
    }
    // A BOM split over chunks.
    {
        QStringDecoder d(E::Utf8);
        QString r;
        r += d(bytes({0xef}));
        r += d(bytes({0xbb}));
        r += d(bytes({0xbf, 'q'}));
        CHECK(r == QString("q"));
    }

    struct Bad {
        QByteArray in;
        QString out;
    };
    const std::vector<Bad> bad = {
        {bytes({0x80}), str({0xfffd})},
        {bytes({0xc0, 0x80}), str({0xfffd, 0xfffd})},
        {bytes({0xc1, 0xbf}), str({0xfffd, 0xfffd})},
        {bytes({0xe0, 0x80, 0x80}), str({0xfffd, 0xfffd, 0xfffd})},
        {bytes({0xed, 0xa0, 0x80}), str({0xfffd, 0xfffd, 0xfffd})},
        {bytes({0xf4, 0x90, 0x80, 0x80}), str({0xfffd, 0xfffd, 0xfffd, 0xfffd})},
        {bytes({0xf5, 0x80}), str({0xfffd, 0xfffd})},
        {bytes({0xff}), str({0xfffd})},
        {bytes({'a', 0xe2, 0x82, 'b'}), str({'a', 0xfffd, 'b'})},
        {bytes({0xe2, 0x82}), str({0xfffd})},
        {bytes({0xf0, 0x9f, 0x98}), str({0xfffd})},
    };
    for (const Bad &b : bad) {
        QStringDecoder d(E::Utf8, QStringConverter::Flag::Stateless);
        CHECK(QString(d(b.in)) == b.out);
        CHECK(d.hasError());
        d.resetState();
        CHECK(!d.hasError());
    }
    {
        QStringDecoder d(E::Utf8, QStringConverter::Flag::ConvertInvalidToNull | QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({'a', 0xff, 'b'}))) == str({'a', 0, 'b'}));
    }
    {
        // Stateful: an incomplete tail is kept, then completed wrongly.
        QStringDecoder d(E::Utf8);
        CHECK(QString(d(bytes({0xe2, 0x82}))).isEmpty());
        CHECK(!d.hasError());
        CHECK(QString(d(bytes({'x'}))) == str({0xfffd, 'x'}));
        CHECK(d.hasError());
    }
    {
        QStringDecoder d(E::Utf8);
        QString r(d(bytes({0xe2, 0x82})));
        d.resetState();
        r = d(bytes({0xac}));
        CHECK(r == str({0xfffd}));
    }

    // Encoder: unpaired surrogates become U+FFFD.
    {
        QStringEncoder e(E::Utf8);
        CHECK(QByteArray(e(str({0xd800, 'a'}))) == bytes({0xef, 0xbf, 0xbd, 'a'}));
        CHECK(e.hasError());
        e.resetState();
        CHECK(!e.hasError());
        CHECK(QByteArray(e(str({0xdc00}))) == bytes({0xef, 0xbf, 0xbd}));
    }
    // Surrogate pair split between two chunks.
    {
        QStringEncoder e(E::Utf8);
        QByteArray r = e(str({'a', 0xd83d}));
        CHECK(r == QByteArray("a"));
        r += QByteArray(e(str({0xde00, 'b'})));
        CHECK(r == QByteArray("a\xf0\x9f\x98\x80" "b"));
        CHECK(!e.hasError());
    }
    {
        QStringEncoder e(E::Utf8, QStringConverter::Flag::Stateless);
        CHECK(QByteArray(e(str({0xd83d}))) == bytes({0xef, 0xbf, 0xbd}));
        CHECK(e.hasError());
    }
    CHECK(QByteArray(QStringEncoder(E::Utf8)(QStringView())).isNull());
    CHECK(QByteArray(QStringEncoder(E::Utf8)(QString(""))).isEmpty());
    CHECK(QByteArray(QStringEncoder(E::System)(u"\u00e9")) == bytes({0xc3, 0xa9}));
}

static void testUtf16() {
    const QString s = str({'a', 0xe9, 0xd83d, 0xde00});
    QStringEncoder le(E::Utf16LE);
    QStringEncoder be(E::Utf16BE);
    CHECK(QByteArray(le(s)) == bytes({'a', 0, 0xe9, 0, 0x3d, 0xd8, 0x00, 0xde}));
    CHECK(QByteArray(be(s)) == bytes({0, 'a', 0, 0xe9, 0xd8, 0x3d, 0xde, 0x00}));
    CHECK(QStringDecoder(E::Utf16LE)(bytes({'a', 0, 0xe9, 0, 0x3d, 0xd8, 0x00, 0xde})).operator QString() == s);
    CHECK(QString(QStringDecoder(E::Utf16BE)(bytes({0, 'a', 0, 0xe9, 0xd8, 0x3d, 0xde, 0x00}))) == s);

    QStringEncoder bomLe(E::Utf16LE, QStringConverter::Flag::WriteBom);
    CHECK(QByteArray(bomLe(u"a")) == bytes({0xff, 0xfe, 'a', 0}));
    QStringEncoder bomBe(E::Utf16BE, QStringConverter::Flag::WriteBom);
    CHECK(QByteArray(bomBe(u"a")) == bytes({0xfe, 0xff, 0, 'a'}));
    QStringEncoder host(E::Utf16, QStringConverter::Flag::WriteBom);
    const QByteArray h = host(u"a");
    CHECK(h == bytes({0xff, 0xfe, 'a', 0}) || h == bytes({0xfe, 0xff, 0, 'a'}));

    // Utf16 detects the byte order from the BOM.
    CHECK(QString(QStringDecoder(E::Utf16)(bytes({0xff, 0xfe, 'a', 0}))) == QString("a"));
    CHECK(QString(QStringDecoder(E::Utf16)(bytes({0xfe, 0xff, 0, 'a'}))) == QString("a"));
    CHECK(QString(QStringDecoder(E::Utf16, QStringConverter::Flag::ConvertInitialBom)(bytes({0xfe, 0xff, 0, 'a'}))) == str({0xfeff, 'a'}));
    // Explicit byte order keeps U+FEFF.
    CHECK(QString(QStringDecoder(E::Utf16LE)(bytes({0xff, 0xfe, 'a', 0}))) == str({0xfeff, 'a'}));

    // Roundtrip of chunks at every byte split.
    const QByteArray src = QByteArray(be(s));
    for (qsizetype cut = 0; cut <= src.size(); ++cut) {
        QStringDecoder d(E::Utf16BE);
        QString r;
        r += d(QByteArrayView(src.constData(), cut));
        r += d(QByteArrayView(src.constData() + cut, src.size() - cut));
        CHECK(r == s);
        CHECK(!d.hasError());
    }
    {
        // BOM and first character split over chunks.
        QStringDecoder d(E::Utf16);
        QString r;
        r += d(bytes({0xfe}));
        r += d(bytes({0xff, 0}));
        r += d(bytes({'k'}));
        CHECK(r == QString("k"));
    }

    // Invalid sequences.
    {
        QStringDecoder d(E::Utf16LE, QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({0x00, 0xd8, 'a', 0}))) == str({0xfffd, 'a'}));
        CHECK(d.hasError());
    }
    {
        QStringDecoder d(E::Utf16LE, QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({0x00, 0xdc}))) == str({0xfffd}));
        CHECK(d.hasError());
    }
    {
        QStringDecoder d(E::Utf16LE, QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({'a', 0, 'b'}))) == str({'a', 0xfffd}));
        CHECK(d.hasError());
    }
    {
        QStringDecoder d(E::Utf16LE, QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({'a', 0, 0x3d, 0xd8}))) == str({'a', 0xfffd}));
    }
    {
        // The stateful decoder waits for the low surrogate.
        QStringDecoder d(E::Utf16LE);
        CHECK(QString(d(bytes({0x3d, 0xd8}))).isEmpty());
        CHECK(QString(d(bytes({0x00, 0xde}))) == str({0xd83d, 0xde00}));
        CHECK(!d.hasError());
    }
    {
        QStringEncoder e(E::Utf16LE);
        CHECK(QByteArray(e(str({0xdc00}))) == bytes({0xfd, 0xff}));
        CHECK(e.hasError());
    }
}

static void testUtf32() {
    const QString s = str({'a', 0xd83d, 0xde00});
    QStringEncoder le(E::Utf32LE);
    QStringEncoder be(E::Utf32BE);
    CHECK(QByteArray(le(s)) == bytes({'a', 0, 0, 0, 0x00, 0xf6, 0x01, 0x00}));
    CHECK(QByteArray(be(s)) == bytes({0, 0, 0, 'a', 0x00, 0x01, 0xf6, 0x00}));
    CHECK(QString(QStringDecoder(E::Utf32LE)(bytes({'a', 0, 0, 0, 0x00, 0xf6, 0x01, 0x00}))) == s);
    CHECK(QString(QStringDecoder(E::Utf32BE)(bytes({0, 0, 0, 'a', 0x00, 0x01, 0xf6, 0x00}))) == s);
    QStringEncoder bom(E::Utf32BE, QStringConverter::Flag::WriteBom);
    CHECK(QByteArray(bom(u"a")) == bytes({0, 0, 0xfe, 0xff, 0, 0, 0, 'a'}));
    CHECK(QString(QStringDecoder(E::Utf32)(bytes({0xff, 0xfe, 0, 0, 'a', 0, 0, 0}))) == QString("a"));
    CHECK(QString(QStringDecoder(E::Utf32)(bytes({0, 0, 0xfe, 0xff, 0, 0, 0, 'a'}))) == QString("a"));

    const QByteArray src = QByteArray(be(s));
    for (qsizetype cut = 0; cut <= src.size(); ++cut) {
        QStringDecoder d(E::Utf32BE);
        QString r;
        r += d(QByteArrayView(src.constData(), cut));
        r += d(QByteArrayView(src.constData() + cut, src.size() - cut));
        CHECK(r == s);
        CHECK(!d.hasError());
    }
    {
        QStringDecoder d(E::Utf32BE, QStringConverter::Flag::Stateless);
        CHECK(QString(d(bytes({0, 0x11, 0, 0, 0, 0, 0xd8, 0x00, 0, 0, 0, 'a', 0, 0}))) == str({0xfffd, 0xfffd, 'a', 0xfffd}));
        CHECK(d.hasError());
    }
    {
        QStringDecoder d(E::Utf32);
        QString r;
        r += d(bytes({0xff, 0xfe}));
        r += d(bytes({0, 0, 'z'}));
        r += d(bytes({0, 0, 0}));
        CHECK(r == QString("z"));
    }
}

static void testLatin1AndTables() {
    CHECK(QByteArray(QStringEncoder(E::Latin1)(str({'a', 0xe9, 0x20ac, 0xd83d, 0xde00}))) == bytes({'a', 0xe9, '?', '?'}));
    QStringEncoder l1(E::Latin1);
    CHECK(QByteArray(l1(u"\u20ac")) == QByteArray("?"));
    CHECK(l1.hasError());
    QStringEncoder l1n(E::Latin1, QStringConverter::Flag::ConvertInvalidToNull);
    CHECK(QByteArray(l1n(u"\u20ac")) == bytes({0}));
    CHECK(QString(QStringDecoder(E::Latin1)(bytes({'a', 0xe9, 0xff}))) == str({'a', 0xe9, 0xff}));

    // windows-1252
    QStringDecoder w("windows-1252");
    CHECK(QString(w(bytes({0x80, 'a', 0xe9}))) == str({0x20ac, 'a', 0xe9}));
    CHECK(!w.hasError());
    CHECK(QString(w(bytes({0x81}))) == str({0xfffd}));
    CHECK(w.hasError());
    QStringEncoder we("CP1252");
    CHECK(QByteArray(we(str({0x20ac, 'a', 0xe9}))) == bytes({0x80, 'a', 0xe9}));
    CHECK(!we.hasError());
    CHECK(QByteArray(we(str({0x0416}))) == QByteArray("?"));
    CHECK(we.hasError());

    // ISO-8859-2, ISO-8859-15, windows-1251, KOI8-R
    CHECK(QString(QStringDecoder("ISO-8859-2")(bytes({0xa1, 0xb1}))) == str({0x0104, 0x0105}));
    CHECK(QByteArray(QStringEncoder("latin2")(str({0x0104, 0x0105}))) == bytes({0xa1, 0xb1}));
    CHECK(QString(QStringDecoder("iso-8859-15")(bytes({0xa4}))) == str({0x20ac}));
    CHECK(QString(QStringDecoder("windows-1251")(bytes({0xcf, 0xf0, 0xe8}))) == str({0x041f, 0x0440, 0x0438}));
    CHECK(QString(QStringDecoder("koi8-r")(bytes({0xd0, 0xd2, 0xc9}))) == str({0x043f, 0x0440, 0x0438}));
    CHECK(QByteArray(QStringEncoder("KOI8-R")(str({0x043f, 0x0440, 0x0438}))) == bytes({0xd0, 0xd2, 0xc9}));
    CHECK(QString(QStringDecoder("IBM437")(bytes({0x80}))) == str({0x00c7}));
    CHECK(QString(QStringDecoder("cp866")(bytes({0x80}))) == str({0x0410}));
    CHECK(QString(QStringDecoder("macintosh")(bytes({0x80}))) == str({0x00c4}));

    // Every table roundtrips all of its mapped bytes.
    const char *names[] = {"ISO-8859-2", "ISO-8859-3", "ISO-8859-4", "ISO-8859-5", "ISO-8859-6", "ISO-8859-7", "ISO-8859-8",
                           "ISO-8859-9", "ISO-8859-10", "ISO-8859-11", "ISO-8859-13", "ISO-8859-14", "ISO-8859-15",
                           "ISO-8859-16", "windows-1250", "windows-1251", "windows-1252", "windows-1253", "windows-1254",
                           "windows-1255", "windows-1256", "windows-1257", "windows-1258", "KOI8-R", "KOI8-U", "IBM437",
                           "IBM850", "IBM866", "macintosh"};
    for (const char *n : names) {
        QStringDecoder d(n, QStringConverter::Flag::Stateless);
        QStringEncoder e(n, QStringConverter::Flag::Stateless);
        CHECK(d.isValid() && e.isValid());
        for (int b = 0; b < 256; ++b) {
            const QString u = d(bytes({b}));
            if (u == str({0xfffd})) {
                CHECK(b >= 0x80);
                continue;
            }
            CHECK(u.size() == 1);
            CHECK(QByteArray(e(u)) == bytes({b}));
        }
    }

    QStringDecoder a("ascii");
    CHECK(QString(a(bytes({'a', 0x80}))) == str({'a', 0xfffd}));
    CHECK(QByteArray(QStringEncoder("ascii")(str({'a', 0xe9}))) == QByteArray("a?"));
}

static void testBuffers() {
    QStringEncoder e(E::Utf8);
    const QString s = QString::fromUtf8("h\xc3\xa9llo \xe2\x82\xac");
    QByteArray buf(e.requiredSpace(s.size()), '\0');
    char *end = e.appendToBuffer(buf.data(), s);
    buf.truncate(end - buf.constData());
    CHECK(buf == s.toUtf8());
    CHECK(e.requiredSpace(0) >= 0);

    QStringDecoder d(E::Utf8);
    QString out(d.requiredSpace(buf.size()), QChar());
    QChar *qe = d.appendToBuffer(out.data(), buf);
    out.truncate(qe - out.constData());
    CHECK(out == s);

    const QByteArray raw = QStringEncoder(E::Utf8)(s.constData(), s.size());
    CHECK(raw == buf);
    CHECK(QString(QStringDecoder(E::Utf8)(buf.constData(), buf.size())) == s);

    // Moved converters keep working.
    QStringDecoder d2(std::move(d));
    CHECK(d2.isValid());
    CHECK(QString(d2("xyz")) == QString("xyz"));

    // Empty input is fine.
    CHECK(QString(QStringDecoder(E::Utf8)(QByteArray(""))).isEmpty());
    CHECK(QString(QStringDecoder(E::Utf8)(QByteArray(""))).isNull() == false);
}

static void testSniffing() {
    CHECK(E::encodingForData(bytes({0xef, 0xbb, 0xbf, 'a'})) == E::Utf8);
    CHECK(E::encodingForData(bytes({0xff, 0xfe, 'a', 0})) == E::Utf16LE);
    CHECK(E::encodingForData(bytes({0xfe, 0xff, 0, 'a'})) == E::Utf16BE);
    CHECK(E::encodingForData(bytes({0xff, 0xfe, 0, 0})) == E::Utf32LE);
    CHECK(E::encodingForData(bytes({0, 0, 0xfe, 0xff})) == E::Utf32BE);
    CHECK(!E::encodingForData(QByteArray("abc")).has_value());
    CHECK(!E::encodingForData(QByteArray()).has_value());
    CHECK(E::encodingForData(QByteArray("<a"), u'<') == E::Utf8);
    CHECK(E::encodingForData(bytes({'<', 0, 'a', 0}), u'<') == E::Utf16LE);
    CHECK(E::encodingForData(bytes({0, '<', 0, 'a'}), u'<') == E::Utf16BE);
    CHECK(E::encodingForData(bytes({'<', 0, 0, 0, 'a'}), u'<') == E::Utf32LE);
    CHECK(E::encodingForData(bytes({0, 0, 0, '<', 0}), u'<') == E::Utf32BE);
    CHECK(!E::encodingForData(QByteArray("abc"), u'<').has_value());
    CHECK(E::encodingForData(bytes({0xc3, 0xa9}), u'\u00e9') == E::Utf8);

    CHECK(E::encodingForHtml(bytes({0xef, 0xbb, 0xbf, '<'})) == E::Utf8);
    CHECK(E::encodingForHtml(QByteArray("<html><head><meta charset=\"utf-8\"></head>")) == E::Utf8);
    CHECK(E::encodingForHtml(QByteArray("<HTML><META CHARSET=ISO-8859-1>")) == E::Latin1);
    CHECK(E::encodingForHtml(QByteArray("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">")) == E::Utf8);
    CHECK(E::encodingForHtml(QByteArray("<meta charset='utf-16le'>")) == E::Utf8);
    CHECK(!E::encodingForHtml(QByteArray("<meta charset=\"windows-1252\">")).has_value());
    CHECK(!E::encodingForHtml(QByteArray("<p>no meta</p>")).has_value());
}

static void testQStringUcs4() {
    const QString s = str({'a', 0xd83d, 0xde00, 0x20ac});
    const QList<uint> u = s.toUcs4();
    CHECK(u.size() == 3);
    CHECK(u[0] == 'a' && u[1] == 0x1f600 && u[2] == 0x20ac);
    CHECK(QString::fromUcs4(u.constData(), u.size()) == s);
    CHECK(QString::fromUcs4(U"a\U0001F600\u20ac") == s);
    CHECK(QString::fromUcs4(U"abc", 2) == QString("ab"));
    CHECK(QString::fromUcs4(static_cast<const char32_t *>(nullptr)).isNull());
    const char32_t bad[] = {U'a', 0x110000, 0xd800, U'b'};
    CHECK(QString::fromUcs4(bad, 4) == str({'a', 0xfffd, 0xfffd, 'b'}));
    CHECK(str({0xd800, 'a', 0xdc00}).toUcs4() == QList<uint>({0xfffd, 'a', 0xfffd}));
    CHECK(s.toStdU32String() == U"a\U0001F600\u20ac");
    CHECK(QString::fromStdU32String(U"a\U0001F600\u20ac") == s);
    CHECK(QString::fromWCharArray(L"a\u20ac") == str({'a', 0x20ac}));
    CHECK(QString::fromStdWString(L"a\u20ac") == str({'a', 0x20ac}));
    CHECK(s.toStdWString() == std::wstring(L"a\U0001F600\u20ac"));
    std::vector<wchar_t> w(static_cast<std::size_t>(s.size()));
    const qsizetype n = s.toWCharArray(w.data());
    CHECK(std::wstring(w.data(), static_cast<std::size_t>(n)) == std::wstring(L"a\U0001F600\u20ac"));
    CHECK(QString().toUcs4().isEmpty());
}

static void testViews() {
    // QUtf8StringView
    const QUtf8StringView u8("h\xc3\xa9llo");
    CHECK(u8.size() == 6);
    CHECK(!u8.isNull() && !u8.isEmpty());
    CHECK(u8.toString() == str({'h', 0xe9, 'l', 'l', 'o'}));
    CHECK(QUtf8StringView().isNull());
    CHECK(QUtf8StringView().toString().isNull());
    CHECK(QUtf8StringView(nullptr).isEmpty());
    CHECK(QUtf8StringView(QByteArray("abc")).size() == 3);
    CHECK(QUtf8StringView(std::string("abc")) == QUtf8StringView("abc"));
    CHECK(QUtf8StringView("abc") < QUtf8StringView("abd"));
    CHECK(QUtf8StringView("abc") != QUtf8StringView("ab"));
    CHECK(QUtf8StringView("abcdef").mid(2, 2) == QUtf8StringView("cd"));
    CHECK(QUtf8StringView("abcdef").left(2) == QUtf8StringView("ab"));
    CHECK(QUtf8StringView("abcdef").right(2) == QUtf8StringView("ef"));
    CHECK(QUtf8StringView("abcdef").first(1) == QUtf8StringView("a"));
    CHECK(QUtf8StringView("abcdef").last(1) == QUtf8StringView("f"));
    CHECK(QUtf8StringView("abcdef").sliced(4) == QUtf8StringView("ef"));
    CHECK(QUtf8StringView("abcdef").chopped(4) == QUtf8StringView("ab"));
    CHECK(QUtf8StringView("abcdef").front() == 'a' && QUtf8StringView("abcdef").back() == 'f');
    CHECK(QUtf8StringView("abcdef").mid(10).isEmpty());
    CHECK(QUtf8StringView("abcdef").mid(-2, 4) == QUtf8StringView("ab"));
    {
        QUtf8StringView v("abcdef");
        v.chop(1);
        v.truncate(3);
        CHECK(v == QUtf8StringView("abc"));
        CHECK(std::string(v.begin(), v.end()) == "abc");
    }

    // QAnyStringView
    const QString qs = QString::fromUtf8("h\xc3\xa9llo");
    const QAnyStringView fromUtf8("h\xc3\xa9llo");
    const QAnyStringView fromU16(u"h\u00e9llo");
    const QAnyStringView fromQString(qs);
    const QAnyStringView fromLatin1(QLatin1StringView("h\xe9llo"));
    const std::string stdString("h\xc3\xa9llo");
    const QByteArray byteArray("h\xc3\xa9llo");
    const QAnyStringView fromStdString(stdString);
    const QAnyStringView fromBa(byteArray);
    const QAnyStringView fromSv{QStringView(qs)};
    CHECK(fromUtf8.isUtf8() && fromUtf8.size() == 6);
    CHECK(fromU16.isUtf16() && fromU16.size() == 5);
    CHECK(fromQString.isUtf16());
    CHECK(fromLatin1.isLatin1() && fromLatin1.size() == 5);
    CHECK(fromSv.isUtf16());
    for (const QAnyStringView &v : {fromUtf8, fromU16, fromQString, fromLatin1, fromStdString, fromBa, fromSv}) {
        CHECK(v.toString() == qs);
        CHECK(v == fromQString);
        CHECK(v == fromUtf8);
        CHECK(!(v != fromU16));
        CHECK(v.compare(QAnyStringView("H\xc3\x89LLO"), Qt::CaseInsensitive) == 0);
        CHECK(qHash(v) == qHash(qs));
    }
    CHECK(fromU16.sizeInBytes() == 10 && fromUtf8.sizeInBytes() == 6);
    CHECK(QAnyStringView().isNull());
    CHECK(QAnyStringView().toString().isNull());
    CHECK(QAnyStringView(nullptr).isEmpty());
    CHECK(QAnyStringView("abc") < QAnyStringView(u"abd"));
    CHECK(QAnyStringView(u"abc") <= QAnyStringView("abc"));
    CHECK(QAnyStringView("abd") > QAnyStringView(QLatin1StringView("abc")));
    CHECK(QAnyStringView("b") >= QAnyStringView("a"));
    CHECK(QAnyStringView("abc") != QAnyStringView("ab"));
    CHECK(QAnyStringView("abc").at(1) == 'b' && QAnyStringView(u"abc")[2] == 'c');
    CHECK(QAnyStringView(u"abc").front() == 'a' && QAnyStringView("abc").back() == 'c');
    CHECK(QAnyStringView(u"abcdef").mid(2, 2) == QAnyStringView(u"cd"));
    CHECK(QAnyStringView("abcdef").left(2) == QAnyStringView("ab"));
    CHECK(QAnyStringView("abcdef").right(2) == QAnyStringView("ef"));
    CHECK(QAnyStringView(u"abcdef").first(1) == QAnyStringView(u"a"));
    CHECK(QAnyStringView(u"abcdef").last(2) == QAnyStringView("ef"));
    CHECK(QAnyStringView("abcdef").sliced(1, 2) == QAnyStringView("bc"));
    CHECK(QAnyStringView("abcdef").sliced(4) == QAnyStringView("ef"));
    CHECK(QAnyStringView("abcdef").chopped(5) == QAnyStringView("a"));
    CHECK(QAnyStringView(u"abcdef").mid(9).isEmpty());
    {
        QAnyStringView v(u"abcdef");
        v.chop(1);
        v.truncate(3);
        CHECK(v == QAnyStringView(u"abc"));
    }
    {
        const QChar c(u'z');
        CHECK(QAnyStringView(c).toString() == QString("z"));
        CHECK(QAnyStringView(qs.constData(), 2).toString() == str({'h', 0xe9}));
        CHECK(QAnyStringView("hello", 2).toString() == QString("he"));
        CHECK(QAnyStringView(std::u16string(u"xy")).toString() == QString("xy"));
        CHECK(QAnyStringView(std::u16string_view(u"xy")).isUtf16());
        CHECK(QAnyStringView(std::string_view("xy")).isUtf8());
        CHECK(QAnyStringView(QByteArrayView("xy")).isUtf8());
        CHECK(QAnyStringView(QUtf8StringView("xy")).isUtf8());
    }
    CHECK(fromUtf8.visit([](auto v) { return v.size(); }) == 6);
    CHECK(fromU16.visit([](auto v) { return v.size(); }) == 5);
    CHECK(fromLatin1.visit([](auto v) { return v.size(); }) == 5);
    CHECK(fromLatin1.visit([](auto v) { return static_cast<int>(sizeof(v) > 0); }) == 1);
    {
        int kind = 0;
        fromUtf8.visit([&kind](auto v) { kind = std::is_same_v<decltype(v), QUtf8StringView> ? 1 : 0; });
        CHECK(kind == 1);
        fromU16.visit([&kind](auto v) { kind = std::is_same_v<decltype(v), QStringView> ? 2 : 0; });
        CHECK(kind == 2);
        fromLatin1.visit([&kind](auto v) { kind = std::is_same_v<decltype(v), QLatin1StringView> ? 3 : 0; });
        CHECK(kind == 3);
    }
}

static void testMatcher() {
    QStringMatcher m(QString("needle"));
    CHECK(m.pattern() == QString("needle"));
    CHECK(m.caseSensitivity() == Qt::CaseSensitive);
    CHECK(m.indexIn(QString("a needle in a haystack")) == 2);
    CHECK(m.indexIn(QString("a needle in a haystack"), 3) == -1);
    CHECK(m.indexIn(QString("needleneedle"), 1) == 6);
    CHECK(m.indexIn(QString("NEEDLE")) == -1);
    CHECK(m.indexIn(QString("needl")) == -1);
    CHECK(m.indexIn(QString("")) == -1);
    m.setCaseSensitivity(Qt::CaseInsensitive);
    CHECK(m.indexIn(QString("a NeEdLe")) == 2);
    CHECK(m.indexIn(QStringView(u"xx NEEDLE")) == 3);
    m.setPattern(QString("abc"));
    CHECK(m.indexIn(QString("xxABCxx")) == 2);
    CHECK(m.indexIn(QString("abc"), -5) == 0);

    QStringMatcher empty;
    CHECK(empty.indexIn(QString("abc")) == 0);
    CHECK(empty.indexIn(QString("abc"), 3) == 3);
    CHECK(empty.indexIn(QString("abc"), 4) == -1);

    // Cross-check against QString::indexOf for many patterns and texts.
    const QString text = QString::fromUtf8("abracadabra abracadabra \xc3\x89\xc3\xa9 \xce\xa3\xcf\x83 \xd0\x94\xd0\xb4 aaaaab aab");
    const QString pats[] = {"a", "ab", "abra", "cad", "dab", "ra a", "aab", "aaaab", "z", "abracadabra abracadabra", QString::fromUtf8("\xc3\xa9"),
                            QString::fromUtf8("\xcf\x83"), QString::fromUtf8("\xd0\xb4")};
    for (const QString &p : pats) {
        for (Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
            QStringMatcher mm(p, cs);
            for (qsizetype from = 0; from <= text.size(); ++from)
                CHECK(mm.indexIn(text, from) == text.indexOf(p, from, cs));
        }
    }
    QStringMatcher fromChars(text.constData(), 3);
    CHECK(fromChars.pattern() == QString("abr"));
    CHECK(fromChars.indexIn(text.constData(), text.size(), 1) == 7);
}

static QStringList toList(const QStringList &l) { return l; }

template <typename T>
static QStringList collect(const T &tok) {
    QStringList r;
    for (QStringView v : tok)
        r.append(v.toString());
    return r;
}

static void testTokenizer() {
    const QString src("a,b,,c");
    CHECK(collect(QStringView(src).tokenize(u',')) == QStringList({"a", "b", "", "c"}));
    CHECK(collect(QStringView(src).tokenize(u',', Qt::SkipEmptyParts)) == QStringList({"a", "b", "c"}));
    CHECK(collect(src.tokenize(QChar(','))) == src.split(','));
    CHECK(collect(qTokenize(src, QString(",,"))) == QStringList({"a,b", "c"}));
    CHECK(collect(qTokenize(src, u",")) == src.split(','));
    CHECK(collect(qTokenize(u"A--b--C", QLatin1StringView("--"))) == QStringList({"A", "b", "C"}));
    CHECK(collect(qTokenize(u"aXbxc", u"x", Qt::CaseInsensitive)) == QStringList({"a", "b", "c"}));
    CHECK(collect(qTokenize(u"aXbxc", u"x", Qt::CaseSensitive)) == QStringList({"aXb", "c"}));
    CHECK(collect(qTokenize(u"aXbxc", u"x", Qt::SkipEmptyParts, Qt::CaseInsensitive)) == QStringList({"a", "b", "c"}));
    CHECK(collect(qTokenize(u"aXbxc", u"x", Qt::CaseInsensitive, Qt::SkipEmptyParts)) == QStringList({"a", "b", "c"}));
    CHECK(collect(QString("a b").tokenize(" ")) == QStringList({"a", "b"}));

    // Same results as QString::split().
    const QString inputs[] = {"", ",", ",,", "a", "a,", ",a", "a,b", "a,,b,", ",,a,,", "abc", "x--y----z--"};
    const QString seps[] = {",", "--", "", "abc", "z"};
    for (const QString &in : inputs) {
        for (const QString &sep : seps) {
            for (Qt::SplitBehavior sb : {Qt::KeepEmptyParts, Qt::SkipEmptyParts}) {
                CHECK(collect(in.tokenize(sep, sb)) == in.split(sep, sb));
                CHECK(collect(QString(in).tokenize(sep, sb)) == in.split(sep, sb));
                CHECK(collect(QStringView(in).tokenize(sep, sb)) == in.split(sep, sb));
            }
        }
        CHECK(collect(in.tokenize(u',')) == in.split(','));
        CHECK(collect(in.tokenize(u',', Qt::SkipEmptyParts)) == in.split(',', Qt::SkipEmptyParts));
    }

    // Tokens are views into the source.
    {
        const QString s("one two");
        auto tok = s.tokenize(u' ');
        auto it = tok.begin();
        CHECK(it->data() == s.constData());
        CHECK((*it).size() == 3);
        ++it;
        CHECK(it->data() == s.constData() + 4);
        auto old = it++;
        CHECK(old != it);
        CHECK(it == tok.end());
        CHECK(tok.cbegin() == tok.begin());
        CHECK(tok.cend() == tok.end());
    }
    // An rvalue string is owned by the tokenizer.
    {
        auto tok = QString("p;q;r").tokenize(u';');
        CHECK(collect(tok) == QStringList({"p", "q", "r"}));
        auto tok2 = QStringView(u"p;q").tokenize(u';');
        CHECK(tok2.toContainer<QList<QString>>().size() == 2);
        const QList<QStringView> views = tok.toContainer();
        CHECK(views.size() == 3 && views[1] == QStringView(u"q"));
        const QStringList strings = tok.toContainer<QStringList>();
        CHECK(strings == QStringList({"p", "q", "r"}));
    }
    const QString constStr("a-b");
    CHECK(collect(std::move(constStr).tokenize(u'-')).size() == 2);
    CHECK(collect(QStringView(u"").tokenize(u',')).size() == 1);
    CHECK(collect(QStringView(u"").tokenize(u',', Qt::SkipEmptyParts)).isEmpty());
    CHECK(toList(collect(QStringView(u"abc").tokenize(u""))) == QStringList({"", "a", "b", "c", ""}));
    int n = 0;
    for (auto part : QStringView(u"1 2 3").tokenize(u' '))
        n += part.toInt();
    CHECK(n == 6);
}

int main() {
    testNames();
    testUtf8();
    testUtf16();
    testUtf32();
    testLatin1AndTables();
    testBuffers();
    testSniffing();
    testQStringUcs4();
    testViews();
    testMatcher();
    testTokenizer();
    if (g_failures == 0)
        std::printf("all text tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
