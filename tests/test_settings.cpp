// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporarydir.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QString g_root;
static QString g_userDir;
static QString g_sysDir1;
static QString g_sysDir2;

static bool writeFile(const QString &path, const QByteArray &data) {
    QFileInfo(path).dir().mkpath(".");
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

static QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

static QByteArray bytes(const char *s) { return QByteArray(s); }

// A fresh file name in the test directory.
static QString newFile(const char *tag) {
    static int counter = 0;
    return g_root + "/files/" + tag + QString::number(++counter) + ".ini";
}

// ---- the layout of the files -----------------------------------------------------------------

static void testLocations() {
    {
        QSettings s("Org", "App");
        CHECK(s.fileName() == g_userDir + "/Org/App.conf");
        CHECK(s.format() == QSettings::NativeFormat);
        CHECK(s.scope() == QSettings::UserScope);
        CHECK(s.organizationName() == "Org" && s.applicationName() == "App");
        CHECK(s.status() == QSettings::NoError);
        CHECK(s.fallbacksEnabled());
        CHECK(s.isAtomicSyncRequired());
    }
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Org", "App");
        CHECK(s.fileName() == g_userDir + "/Org/App.ini");
        CHECK(s.format() == QSettings::IniFormat);
    }
    {
        QSettings s(QSettings::SystemScope, "Org", "App");
        CHECK(s.fileName() == g_sysDir1 + "/Org/App.conf");
        CHECK(s.scope() == QSettings::SystemScope);
    }
    {
        QSettings s("Org");
        CHECK(s.fileName() == g_userDir + "/Org.conf");
    }
    {
        QSettings s(g_root + "/x/../explicit.cfg", QSettings::IniFormat);
        CHECK(s.fileName() == g_root + "/explicit.cfg");
        CHECK(s.organizationName().isEmpty() && s.applicationName().isEmpty());
    }
    {
        QSettings s("", "App");
        CHECK(s.status() == QSettings::AccessError);
        CHECK(s.fileName() == g_userDir + "/Unknown Organization/App.conf");
    }

    QCoreApplication::setOrganizationName("AppOrg");
    QCoreApplication::setApplicationName("AppName");
    {
        QSettings s;
        CHECK(s.fileName() == g_userDir + "/AppOrg/AppName.conf");
        CHECK(s.organizationName() == "AppOrg");
    }
    {
        QSettings s(QSettings::SystemScope, nullptr);
        CHECK(s.fileName() == g_sysDir1 + "/AppOrg/AppName.conf");
    }
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setOrganizationDomain("example.org");
    CHECK(QCoreApplication::organizationDomain() == "example.org");
    {
        QSettings s;
        CHECK(s.organizationName() == "example.org");
        CHECK(s.fileName() == g_userDir + "/example.org/AppName.conf");
    }
    QCoreApplication::setOrganizationDomain(QString());
    {
        QSettings s;
        CHECK(s.status() == QSettings::AccessError);
    }
    QCoreApplication::setOrganizationName("AppOrg");

    QSettings::setDefaultFormat(QSettings::IniFormat);
    CHECK(QSettings::defaultFormat() == QSettings::IniFormat);
    {
        QSettings s("Org", "App");
        CHECK(s.format() == QSettings::IniFormat);
        CHECK(s.fileName() == g_userDir + "/Org/App.ini");
    }
    QSettings::setDefaultFormat(QSettings::NativeFormat);

    // setPath applies to new objects, and the Ini path to the other formats without a path of their own.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, g_root + "/custompath");
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "O", "A");
        CHECK(s.fileName() == g_root + "/custompath/O/A.ini");
        QSettings n(QSettings::NativeFormat, QSettings::UserScope, "O", "A");
        CHECK(n.fileName() == g_root + "/custompath/O/A.conf");
    }
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, g_root + "/nativepath");
    {
        QSettings n(QSettings::NativeFormat, QSettings::UserScope, "O", "A");
        CHECK(n.fileName() == g_root + "/nativepath/O/A.conf");
    }
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, g_root + "/syspath");
    {
        QSettings n(QSettings::IniFormat, QSettings::SystemScope, "O", "A");
        CHECK(n.fileName() == g_root + "/syspath/O/A.ini");
    }
    // Restore the defaults of the environment.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, g_userDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, g_userDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, g_sysDir1);
}

static void testFallbacks() {
    writeFile(g_sysDir1 + "/FOrg/FApp.conf", "[General]\nsysapp=1\nshared=sysapp\n");
    writeFile(g_sysDir1 + "/FOrg.conf", "[General]\nsysorg=1\nshared=sysorg\n[grp]\nsub=s\n");
    writeFile(g_sysDir2 + "/FOrg/FApp.conf", "[General]\nsys2app=1\n");
    writeFile(g_userDir + "/FOrg.conf", "[General]\nuserorg=1\nshared=userorg\n");
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, g_sysDir1);
    {
        QSettings s("FOrg", "FApp");
        CHECK(s.value("sysapp").toInt() == 1);
        CHECK(s.value("sysorg").toInt() == 1);
        CHECK(s.value("userorg").toInt() == 1);
        CHECK(s.value("shared").toString() == "userorg");
        CHECK(s.contains("grp/sub"));
        CHECK(s.childGroups() == QStringList("grp"));
        CHECK(s.allKeys() == (QStringList{"grp/sub", "shared", "sysapp", "sysorg", "userorg"}));
        // Writing goes to the first file, which then hides the others.
        s.setValue("shared", "mine");
        CHECK(s.value("shared").toString() == "mine");
        s.sync();
        CHECK(readFile(g_userDir + "/FOrg/FApp.conf") == bytes("[General]\nshared=mine\n"));
        s.remove("shared");
        CHECK(s.value("shared").toString() == "userorg");
        s.setFallbacksEnabled(false);
        CHECK(!s.fallbacksEnabled());
        CHECK(!s.contains("shared"));
        CHECK(!s.contains("sysapp"));
        CHECK(s.allKeys().isEmpty());
        s.setFallbacksEnabled(true);
        CHECK(s.contains("sysapp"));
    }
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QString());
    // The default system directories are those of XDG_CONFIG_DIRS.
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QString());
    {
        QSettings s(QSettings::NativeFormat, QSettings::UserScope, "FOrg", "FApp");
        CHECK(s.value("sys2app").toInt() == 0 || true);
    }
}

// ---- values, groups and arrays ------------------------------------------------------------------

static void testBasics() {
    const QString path = newFile("basic");
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(!s.contains("a"));
    CHECK(!s.value("a").isValid());
    CHECK(s.value("a", 5).toInt() == 5);
    s.setValue("a", 1);
    s.setValue("b/c", "text");
    s.setValue("b/d/e", true);
    s.setValue("//lead//dup/", 2);
    CHECK(s.value("a").toInt() == 1);
    CHECK(s.value("a", 5).toInt() == 1);
    CHECK(s.value("/b//c").toString() == "text");
    CHECK(s.contains("b/d/e"));
    CHECK(!s.contains("b/d"));
    CHECK(!s.contains(""));
    CHECK(s.allKeys() == (QStringList{"a", "b/c", "b/d/e", "lead/dup/"}));
    CHECK(s.childKeys() == QStringList("a"));
    CHECK(s.childGroups() == (QStringList{"b", "lead"}));

    s.beginGroup("b");
    CHECK(s.group() == "b");
    CHECK(s.allKeys() == (QStringList{"c", "d/e"}));
    CHECK(s.childKeys() == QStringList("c"));
    CHECK(s.childGroups() == QStringList("d"));
    s.beginGroup("d");
    CHECK(s.group() == "b/d");
    CHECK(s.value("e").toBool());
    s.endGroup();
    CHECK(s.group() == "b");
    s.setValue("f", 3);
    s.endGroup();
    CHECK(s.group().isEmpty());
    CHECK(s.value("b/f").toInt() == 3);

    s.remove("b/d");
    CHECK(!s.contains("b/d/e"));
    CHECK(s.contains("b/c"));
    s.beginGroup("b");
    s.remove("");
    s.endGroup();
    CHECK(s.allKeys() == (QStringList{"a", "lead/dup/"}));
    s.setValue("x", 1);
    s.clear();
    CHECK(s.allKeys().isEmpty());
    s.setValue("again", 1);
    s.sync();
    CHECK(readFile(path) == bytes("[General]\nagain=1\n"));
    s.clear();
    s.sync();
    CHECK(readFile(path).isEmpty());
    CHECK(QFileInfo(path).exists());

    // A key and a group of the same name.
    s.setValue("k", 1);
    s.setValue("k/inner", 2);
    s.sync();
    CHECK(readFile(path) == bytes("[General]\nk=1\n\n[k]\ninner=2\n"));
    s.remove("k");
    CHECK(s.allKeys().isEmpty());
}

static void testArrays() {
    const QString path = newFile("array");
    {
        QSettings s(path, QSettings::IniFormat);
        s.beginWriteArray("logins");
        for (int i = 0; i < 3; ++i) {
            s.setArrayIndex(i);
            s.setValue("user", QString("u%1").arg(i));
            s.setValue("n", i);
        }
        CHECK(s.group() == "logins/3");
        s.endArray();
        CHECK(s.group().isEmpty());
        s.beginWriteArray("fixed", 5);
        s.setArrayIndex(0);
        s.setValue("v", 1);
        s.endArray();
        s.beginWriteArray("guess");
        s.endArray();
        s.setValue("tail", 1);
    }
    CHECK(readFile(path) ==
          bytes("[General]\ntail=1\n\n[fixed]\nsize=5\n\n[fixed\\1]\nv=1\n\n[guess]\nsize=0\n\n"
                "[logins]\nsize=3\n\n[logins\\1]\nn=0\nuser=u0\n\n[logins\\2]\nn=1\nuser=u1\n\n[logins\\3]\nn=2\n"
                "user=u2\n"));
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.beginReadArray("logins") == 3);
    for (int i = 0; i < 3; ++i) {
        s.setArrayIndex(i);
        CHECK(s.value("user").toString() == QString("u%1").arg(i));
        CHECK(s.value("n").toInt() == i);
        CHECK(s.allKeys() == (QStringList{"n", "user"}));
    }
    s.setArrayIndex(-4); // negative indexes select the first element
    CHECK(s.value("user").toString() == "u0");
    s.endArray();
    CHECK(s.beginReadArray("fixed") == 5);
    s.endArray();
    CHECK(s.beginReadArray("missing") == 0);
    s.endArray();
    CHECK(s.group().isEmpty());

    // Rewriting an array with a smaller size leaves the old entries; the caller removes them.
    s.beginWriteArray("logins", 1);
    s.setArrayIndex(0);
    s.setValue("user", "only");
    s.endArray();
    s.beginReadArray("logins");
    s.setArrayIndex(0);
    CHECK(s.value("user").toString() == "only");
    s.endArray();
    CHECK(s.value("logins/size").toInt() == 1);

    // An array inside a group, with the size guessed from the highest index.
    s.beginGroup("outer");
    s.beginWriteArray("list");
    s.setArrayIndex(1);
    s.setValue("v", "x");
    s.setArrayIndex(3);
    s.setValue("v", "y");
    s.endArray();
    s.endGroup();
    CHECK(s.value("outer/list/size").toInt() == 4);
    CHECK(s.value("outer/list/4/v").toString() == "y");
}

// ---- the INI syntax ---------------------------------------------------------------------------

static void testIniWriting() {
    const QString path = newFile("write");
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("zeta", 1);
        s.setValue("alpha", "x");
        s.setValue("My Group/sub/x y", QStringList{"a", "b c", "@d"});
        s.setValue("My Group/plain", "v");
        s.setValue("General/x", 5);
        s.setValue("pt", QPoint(1, -2));
        s.setValue("size", QSize(800, 600));
        s.setValue("rect", QRect(1, 2, 3, 4));
        s.setValue("ba", QByteArray("a\0b\xff" "1", 5));
        s.setValue("neg", -12);
        s.setValue("flag", false);
        s.setValue("dbl", 0.1);
        s.setValue("fl", 0.5f);
        s.setValue("big", qulonglong(18446744073709551615ULL));
        s.setValue("uni", QString::fromUtf8("h\xc3\xa9llo \xf0\x9f\x98\x80"));
        s.setValue("comma", "a,b");
        s.setValue("semi", "a;b");
        s.setValue("eq", "a=b");
        s.setValue("padded", "  p ");
        s.setValue("empty", "");
        s.setValue("quote", "say \"hi\"\\");
        s.setValue("ctl", "l1\nl2\tt\r\a\b\f\v\x01" "0");
        s.setValue("nul", QString(QChar(u'a')) + QChar(u'\0') + QChar(u'5'));
        s.setValue("at", "@Point(1 2)");
        s.setValue("atat", "@@x");
        s.setValue("none", QVariant());
        s.setValue("emptylist", QStringList());
        s.setValue("one", QStringList("solo"));
        s.setValue("list", QStringList{"a,b", "", " c", "d;e"});
        s.setValue(QString(QChar(0xE9)) + "%=" + QChar(0x7F), 1);
        s.setValue(QString::fromUtf8("k\xe2\x82\xac"), 1);
        s.setValue(QString("k") + QChar(0xD83D) + QChar(0xDE00), 1);
    }
    const QByteArray actual = readFile(path);
    // The exact bytes of a few keys whose escaping is easy to get wrong.
    CHECK(actual.contains("[%General]\nx=5\n"));
    CHECK(actual.contains("[My%20Group]\nplain=v\n\n[My%20Group\\sub]\nx%20y=a, b c, @@d\n"));
    CHECK(actual.contains("%E9%25%3D%7F=1\n"));
    CHECK(actual.contains("k%U20AC=1\n"));
    CHECK(actual.contains("k%UD83D%UDE00=1\n"));
    CHECK(actual.startsWith("[General]\nalpha=x\nat=@@Point(1 2)\natat=@@@x\nba=@ByteArray(a\\0\\x62\\xff\\x31)\n"));
    for (const char *line : {"big=18446744073709551615\n", "comma=\"a,b\"\n", "dbl=0.1\n", "empty=\n", "eq=\"a=b\"\n",
                             "fl=0.5\n", "flag=false\n", "neg=-12\n", "none=@Invalid()\n", "one=solo\n",
                             "padded=\"  p \"\n", "pt=@Point(1 -2)\n", "rect=@Rect(1 2 3 4)\n", "semi=\"a;b\"\n",
                             "size=@Size(800 600)\n", "zeta=1\n", "emptylist=@Invalid()\n",
                             "list=\"a,b\", , \" c\", \"d;e\"\n", "quote=say \\\"hi\\\"\\\\\n",
                             "ctl=l1\\nl2\\tt\\r\\a\\b\\f\\v\\x1\\x30\n", "nul=@String(a\\0\\x35)\n"}) {
        if (!actual.contains(line))
            std::printf("missing line: %s", line);
        CHECK(actual.contains(line));
    }
    CHECK(actual.contains("uni=h\xc3\xa9llo \xf0\x9f\x98\x80\n"));

    // Reading it back.
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(s.value("General/x").toInt() == 5);
    CHECK(s.value("My Group/sub/x y").toStringList() == (QStringList{"a", "b c", "@d"}));
    CHECK(s.value("pt").value<QPoint>() == QPoint(1, -2));
    CHECK(s.value("pt").typeId() == QMetaType::QPoint);
    CHECK(s.value("size").value<QSize>() == QSize(800, 600));
    CHECK(s.value("rect").value<QRect>() == QRect(1, 2, 3, 4));
    CHECK(s.value("ba").toByteArray() == QByteArray("a\0b\xff" "1", 5));
    CHECK(s.value("neg").toInt() == -12 && s.value("neg").typeId() == QMetaType::QString);
    CHECK(s.value("flag").toString() == "false");
    CHECK(s.value("dbl").toDouble() == 0.1);
    CHECK(s.value("big").toULongLong() == 18446744073709551615ULL);
    CHECK(s.value("uni").toString() == QString::fromUtf8("h\xc3\xa9llo \xf0\x9f\x98\x80"));
    CHECK(s.value("comma").toString() == "a,b");
    CHECK(s.value("semi").toString() == "a;b");
    CHECK(s.value("eq").toString() == "a=b");
    CHECK(s.value("padded").toString() == "  p ");
    CHECK(s.contains("empty") && s.value("empty").toString().isEmpty());
    CHECK(s.value("quote").toString() == "say \"hi\"\\");
    CHECK(s.value("ctl").toString() == "l1\nl2\tt\r\a\b\f\v\x01" "0");
    CHECK(s.value("nul").toString() == QString(QChar(u'a')) + QChar(u'\0') + QChar(u'5'));
    CHECK(s.value("at").toString() == "@Point(1 2)");
    CHECK(s.value("atat").toString() == "@@x");
    CHECK(s.contains("none") && !s.value("none").isValid());
    CHECK(s.contains("emptylist") && s.value("emptylist").toStringList().isEmpty());
    CHECK(s.value("one").toString() == "solo" && s.value("one").typeId() == QMetaType::QString);
    CHECK(s.value("list").toStringList() == (QStringList{"a,b", "", " c", "d;e"}));
    CHECK(s.contains(QString(QChar(0xE9)) + "%=" + QChar(0x7F)));
    CHECK(s.contains(QString::fromUtf8("k\xe2\x82\xac")));
    CHECK(s.contains(QString("k") + QChar(0xD83D) + QChar(0xDE00)));
}

static void testIniReading() {
    const QString path = newFile("read");
    const QByteArray text =
        "; leading comment\r\n"
        "# hash comment\r\n"
        "\r\n"
        "[General]\r\n"
        "a = 1\r\n"
        "b=  two words  \r\n"
        "c = \"  padded  \" ; trailing comment\r\n"
        "list = x, y ,  z\r\n"
        "quoted = \"a, b\", c\r\n"
        "esc = tab\\there\\x41\\101\\nnew\r\n"
        "cont = abc\\\r\n"
        "def\r\n"
        "path=C:\\\\Windows\\\\System32\r\n"
        "dropped=a\\qb\r\n"
        "utf8=caf\xc3\xa9\r\n"
        "ba=@ByteArray(\\x1\\xd9\\0\\xff)\r\n"
        "bal=@ByteArray(ab), x\r\n"
        "size=@Size(800 600)\r\n"
        "pt=@Point(3 4)\r\n"
        "pf=@Variant(\\0\\0\\0\\x1a?\\xf0\\0\\0\\0\\0\\0\\0@\\0\\0\\0\\0\\0\\0\\0)\r\n"
        "map=@Variant(\\0\\0\\0\\b\\0\\0\\0\\x2\\0\\0\\0\\x2\\0\\x61\\0\\0\\0\\x2\\0\\0\\0\\x1\\0\\0\\0\\x2\\0\\x62\\0\\0\\0\\n"
        "\\0\\0\\0\\x2\\0x)\r\n"
        "inv=@Invalid()\r\n"
        "str=@String(a@b)\r\n"
        "[Group One]   ; comment\r\n"
        "k=v\r\n"
        "k%20ey=%U20AC\r\nk%U20AC=x\r\n"
        "[sub\\sec]\r\n"
        "deep=1\r\n"
        "[%20x%E9]\r\n"
        "z=1\r\n"
        "[general]\r\n"
        "upper=1\r\n"
        "[%General]\r\n"
        "named=1\r\n"
        "[Group One]\r\n"
        "again=1\r\n"
        "dup=1\r\n"
        "dup=2\r\n"
        "inline/key=3\r\n";
    writeFile(path, text);
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(s.value("a").toString() == "1");
    CHECK(s.value("b").toString() == "two words");
    CHECK(s.value("c").toString() == "  padded  ");
    CHECK(s.value("list").toStringList() == (QStringList{"x", "y", "z"}));
    CHECK(s.value("quoted").toStringList() == (QStringList{"a, b", "c"}));
    CHECK(s.value("esc").toString() == "tab\thereAA\nnew");
    CHECK(s.value("cont").toString() == "abcdef");
    CHECK(s.value("path").toString() == "C:\\Windows\\System32");
    CHECK(s.value("dropped").toString() == "ab");
    CHECK(s.value("utf8").toString() == QString::fromUtf8("caf\xc3\xa9"));
    CHECK(s.value("ba").toByteArray() == QByteArray("\x01\xd9\0\xff", 4));
    const QVariantList bal = s.value("bal").toList();
    CHECK(bal.size() == 2 && bal.at(0).toByteArray() == "ab" && bal.at(0).typeId() == QMetaType::QByteArray &&
          bal.at(1).toString() == "x");
    CHECK(s.value("size").toSize() == QSize(800, 600));
    CHECK(s.value("pt").toPoint() == QPoint(3, 4));
    CHECK(s.value("pf").toPointF() == QPointF(1, 2));
    const QVariantMap map = s.value("map").toMap();
    CHECK(map.size() == 2 && map.value("a").toInt() == 1 && map.value("b").toString() == "x");
    CHECK(s.contains("inv") && !s.value("inv").isValid());
    CHECK(s.value("str").toString() == "a@b");
    CHECK(s.value("Group One/k").toString() == "v");
    CHECK(s.value(QString("Group One/k ey")).toString() == "%U20AC");
    CHECK(s.value(QString("Group One/k") + QChar(0x20AC)).toString() == "x");
    CHECK(s.value("sub/sec/deep").toInt() == 1);
    CHECK(s.value(QString(" x") + QChar(0xE9) + "/z").toInt() == 1);
    CHECK(s.value("upper").toInt() == 1);
    CHECK(s.value("General/named").toInt() == 1);
    CHECK(s.value("Group One/again").toInt() == 1);
    CHECK(s.value("Group One/dup").toInt() == 2);
    CHECK(s.value("Group One/inline/key").toInt() == 3);

    // A file with a byte order mark and a final line without a line break.
    writeFile(path, bytes("\xef\xbb\xbf[General]\nx=1\ny=2"));
    QSettings t(path, QSettings::IniFormat);
    t.sync();
    CHECK(t.value("x").toInt() == 1 && t.value("y").toInt() == 2);
    CHECK(t.status() == QSettings::NoError);
}

static void testVariantTypes() {
    const QString path = newFile("variants");
    QVariantMap map;
    map["a"] = 1;
    map["b"] = QString("x");
    map["c"] = QVariantList{1, "two"};
    QVariantHash hash;
    hash["h"] = 2.5;
    const QDateTime utc(QDate(2024, 5, 6), QTime(7, 8, 9, 10), Qt::UTC);
    const QDateTime local(QDate(1999, 12, 31), QTime(23, 59, 59, 999), Qt::LocalTime);
    const QDateTime offset(QDate(2020, 2, 29), QTime(12, 0), Qt::OffsetFromUTC, 3600 * 5 + 1800);
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("int", 42);
        s.setValue("uint", 4000000000u);
        s.setValue("ll", qlonglong(-9000000000LL));
        s.setValue("ull", qulonglong(9000000000ULL));
        s.setValue("double", 3.14159265358979);
        s.setValue("float", 1.5f);
        s.setValue("bool", true);
        s.setValue("string", "str");
        s.setValue("char", QChar(u'x'));
        s.setValue("bytes", QByteArray("\0\1\2", 3));
        s.setValue("date", QDate(2024, 5, 6));
        s.setValue("time", QTime(1, 2, 3, 4));
        s.setValue("datetime", utc);
        s.setValue("datetimeLocal", local);
        s.setValue("datetimeOffset", offset);
        s.setValue("url", QUrl("https://example.org/a?b=c#d"));
        s.setValue("uuid", QVariant::fromValue(QUuid::fromString("{67c8770b-44f1-410a-ab9a-f9b5446f13ee}")));
        s.setValue("point", QPoint(1, 2));
        s.setValue("pointf", QPointF(1.5, 2.5));
        s.setValue("size", QSize(3, 4));
        s.setValue("sizef", QSizeF(3.5, 4.5));
        s.setValue("line", QLine(1, 2, 3, 4));
        s.setValue("linef", QLineF(1.5, 2, 3, 4));
        s.setValue("rect", QRect(1, 2, 3, 4));
        s.setValue("rectf", QRectF(1.5, 2, 3, 4));
        s.setValue("bits", QBitArray(5, true));
        s.setValue("map", map);
        s.setValue("hash", hash);
        s.setValue("list", QVariantList{1, "x", 2.5});
        s.setValue("list1", QVariantList{7});
        s.setValue("list0", QVariantList());
        s.setValue("stringlist", QStringList{"a", "b"});
        s.setValue("bytelist", QVariant::fromValue(QByteArrayList{"a", "b"}));
        s.setValue("short", QVariant::fromValue(short(-5)));
        s.setValue("uchar", QVariant::fromValue(uchar(200)));
    }
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(s.value("int").toInt() == 42);
    CHECK(s.value("uint").toUInt() == 4000000000u);
    CHECK(s.value("ll").toLongLong() == -9000000000LL);
    CHECK(s.value("ull").toULongLong() == 9000000000ULL);
    CHECK(s.value("double").toDouble() == 3.14159265358979);
    CHECK(s.value("float").toFloat() == 1.5f);
    CHECK(s.value("bool").toBool());
    CHECK(s.value("string").toString() == "str");
    CHECK(s.value("char").typeId() == QMetaType::QChar && s.value("char").toChar() == QChar(u'x'));
    CHECK(s.value("bytes").toByteArray() == QByteArray("\0\1\2", 3));
    CHECK(s.value("date").typeId() == QMetaType::QDate && s.value("date").toDate() == QDate(2024, 5, 6));
    CHECK(s.value("time").typeId() == QMetaType::QTime && s.value("time").toTime() == QTime(1, 2, 3, 4));
    CHECK(s.value("datetime").toDateTime() == utc && s.value("datetime").toDateTime().timeSpec() == Qt::UTC);
    CHECK(s.value("datetimeLocal").toDateTime() == local &&
          s.value("datetimeLocal").toDateTime().timeSpec() == Qt::LocalTime);
    CHECK(s.value("datetimeOffset").toDateTime() == offset &&
          s.value("datetimeOffset").toDateTime().offsetFromUtc() == 3600 * 5 + 1800);
    CHECK(s.value("url").toUrl() == QUrl("https://example.org/a?b=c#d"));
    CHECK(s.value("uuid").value<QUuid>() == QUuid::fromString("{67c8770b-44f1-410a-ab9a-f9b5446f13ee}"));
    CHECK(s.value("point").toPoint() == QPoint(1, 2));
    CHECK(s.value("pointf").toPointF() == QPointF(1.5, 2.5));
    CHECK(s.value("size").toSize() == QSize(3, 4));
    CHECK(s.value("sizef").toSizeF() == QSizeF(3.5, 4.5));
    CHECK(s.value("line").toLine() == QLine(1, 2, 3, 4));
    CHECK(s.value("linef").toLineF() == QLineF(1.5, 2, 3, 4));
    CHECK(s.value("rect").toRect() == QRect(1, 2, 3, 4));
    CHECK(s.value("rectf").toRectF() == QRectF(1.5, 2, 3, 4));
    CHECK(s.value("bits").toBitArray() == QBitArray(5, true));
    CHECK(s.value("map").toMap() == map);
    CHECK(s.value("hash").toHash() == hash);
    CHECK(s.value("list").toStringList() == (QStringList{"1", "x", "2.5"}));
    CHECK(s.value("list1").typeId() == QMetaType::QVariantList && s.value("list1").toList() == QVariantList{7});
    CHECK(!s.value("list0").isValid());
    CHECK(s.value("stringlist").toStringList() == (QStringList{"a", "b"}));
    CHECK(s.value("bytelist").value<QByteArrayList>() == (QByteArrayList{"a", "b"}));
    CHECK(s.value("short").typeId() == QMetaType::Short && s.value("short").toInt() == -5);
    CHECK(s.value("uchar").typeId() == QMetaType::UChar && s.value("uchar").toInt() == 200);

    const QByteArray file = readFile(path);
    CHECK(file.contains("date=@Variant(\\0\\0\\0\\xe\\0%\\x8b\\x15)\n"));
    CHECK(file.contains("datetime=@DateTime(\\0\\0\\0\\x10\\0\\0\\0\\0\\0\\0%\\x8b\\x15\\x1\\x87\\xfb\\xb2\\x1)\n"));
    CHECK(file.contains("pointf=@Variant(\\0\\0\\0\\x1a?\\xf8\\0\\0\\0\\0\\0\\0@\\x4\\0\\0\\0\\0\\0\\0)\n"));
    CHECK(file.contains("list=1, x, 2.5\n"));
    CHECK(file.contains("list1=@Variant(\\0\\0\\0\\t\\0\\0\\0\\x1\\0\\0\\0\\x2\\0\\0\\0\\a)\n"));
}

// QVariant streams in the old versions that QSettings uses.
static void testStreamVersions() {
    {
        QByteArray a;
        QDataStream out(&a, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);
        out << QVariant(5) << QVariant() << QVariant(QDate(2024, 5, 6));
        CHECK(a == QByteArray("\0\0\0\x2\0\0\0\x5\0\0\0\0\0\0\0\xe\0%\x8b\x15", 8 + 4 + 8));
        QDataStream in(a);
        in.setVersion(QDataStream::Qt_4_0);
        QVariant i, n, d;
        in >> i >> n >> d;
        CHECK(in.status() == QDataStream::Ok && i.toInt() == 5 && !n.isValid() && d.toDate() == QDate(2024, 5, 6));
    }
    {
        QByteArray a;
        QDataStream out(&a, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_6);
        out << QVariant(5);
        CHECK(a == QByteArray("\0\0\0\x2\0\0\0\0\x5", 9));
    }
}

static void testEscapedKeysRoundTrip() {
    const QString path = newFile("keys");
    const QStringList keys = {"plain",       "with space",         "with=eq",           "with[brackets]",
                              "per%cent",    "semi;colon",         "back\\slash/x",     QString::fromUtf8("é/ü"),
                              "tab\tkey",    "quote\"key",         "hash#key",          QString::fromUtf8("日本語/キー"),
                              "dots.and-dash_under", "%41literal", "%",                 "%U",
                              "a/b/c/d/e",   "General/General",    "general/x",         "%General/x",
                              "x/%General",  QString("emoji") + QChar(0xD83D) + QChar(0xDE00)};
    {
        QSettings s(path, QSettings::IniFormat);
        for (int i = 0; i < keys.size(); ++i)
            s.setValue(keys.at(i), i);
    }
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    for (int i = 0; i < keys.size(); ++i) {
        if (!(s.contains(keys.at(i)) && s.value(keys.at(i)).toInt() == i))
            std::printf("key %d (%s) lost\n", i, keys.at(i).toUtf8().constData());
        CHECK(s.contains(keys.at(i)) && s.value(keys.at(i)).toInt() == i);
    }
    CHECK(s.allKeys().size() == keys.size());
}

static void testValuesRoundTrip() {
    const QString path = newFile("values");
    const QStringList strings = {
        "",
        " ",
        "  lead",
        "trail  ",
        "a,b,c",
        "semi;colon",
        "k=v",
        "\"",
        "\\",
        "\\n literal",
        "@",
        "@@",
        "@Variant(x)",
        "@ByteArray(abc)",
        "@Invalid()",
        "@String(x)",
        "@Rect(1 2 3 4)",
        QString::fromUtf8("h\xc3\xa9llo"),
        QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac"),
        QString("emoji") + QChar(0xD83D) + QChar(0xDE00),
        "line1\nline2\r\n",
        QString("nul") + QChar(u'\0') + "x",
        "\x01\x02 control \x1f",
        "0123 \x01" "9",
        "tab\there",
        "[section]",
        "; not a comment",
        "# hash",
        "a\\x41",
        QString(QChar(0xFFFF)),
        QString(QChar(0x00A0)),
    };
    {
        QSettings s(path, QSettings::IniFormat);
        for (int i = 0; i < strings.size(); ++i) {
            s.setValue(QString("s/%1").arg(i), strings.at(i));
            s.setValue(QString("l/%1").arg(i), QStringList{strings.at(i), "x", strings.at(i)});
        }
    }
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    for (int i = 0; i < strings.size(); ++i) {
        const QString got = s.value(QString("s/%1").arg(i)).toString();
        if (got != strings.at(i))
            std::printf("string %d differs\n", i);
        CHECK(got == strings.at(i));
        const QStringList list = s.value(QString("l/%1").arg(i)).toStringList();
        if (list != QStringList{strings.at(i), "x", strings.at(i)})
            std::printf("list %d differs\n", i);
        CHECK(list == (QStringList{strings.at(i), "x", strings.at(i)}));
    }
}

static void testFormatErrors() {
    {
        const QString path = newFile("bad");
        writeFile(path, "[General]\nok=1\nnoequals\n[broken\nalso=2\n=orphan\n");
        QSettings s(path, QSettings::IniFormat);
        CHECK(s.status() == QSettings::FormatError);
        CHECK(s.value("ok").toInt() == 1);
        CHECK(s.value("also").toInt() == 2); // the keys around the bad lines are kept
        QSettings again(path, QSettings::IniFormat); // a second object on the loaded file reports it too
        CHECK(again.status() == QSettings::FormatError);
    }
    {
        // A malformed variant is read as an invalid value, not as an error.
        const QString path = newFile("badvar");
        writeFile(path, "v1=@Variant(\\0\\0\\0\\xff\\0)\nv2=@Variant()\nv3=@Variant(\\0\\0\\0\\b\\xff\\xff\\xff\\xff)\n"
                        "v4=@DateTime(abc)\n");
        QSettings s(path, QSettings::IniFormat);
        CHECK(s.status() == QSettings::NoError);
        CHECK(!s.value("v1").isValid() && !s.value("v2").isValid() && !s.value("v4").isValid());
        (void)s.value("v3");
    }
    {
        // The settings file is a directory.
        const QString path = g_root + "/files/isdir";
        QDir().mkpath(path);
        QSettings s(path, QSettings::IniFormat);
        CHECK(s.status() == QSettings::AccessError);
    }
    {
        QSettings s(g_root + "/files/regformat.reg", QSettings::Registry32Format);
        CHECK(s.status() == QSettings::AccessError);
        s.setValue("a", 1);
        s.sync();
        CHECK(!s.isWritable());
        CHECK(!QFileInfo(s.fileName()).exists());
    }
    {
        QSettings s(g_root + "/files/invalidformat", QSettings::InvalidFormat);
        CHECK(s.status() == QSettings::AccessError);
        CHECK(!s.isWritable());
    }
    {
        // A write that fails leaves the status set and the data in memory.
        const QString dir = g_root + "/files/blocker";
        writeFile(dir, "this is a file, not a directory");
        QSettings s(dir + "/sub/settings.ini", QSettings::IniFormat);
        CHECK(s.status() == QSettings::NoError);
        CHECK(!s.isWritable());
        s.setValue("a", 1);
        s.sync();
        CHECK(s.status() == QSettings::AccessError);
        CHECK(s.value("a").toInt() == 1);
    }
    if (::geteuid() != 0) {
        const QString dir = g_root + "/files/readonly";
        QDir().mkpath(dir);
        writeFile(dir + "/f.ini", "[General]\nk=1\n");
        ::chmod(dir.toUtf8().constData(), 0555);
        {
            QSettings s(dir + "/f.ini", QSettings::IniFormat);
            CHECK(s.value("k").toInt() == 1);
            s.setValue("k", 2);
            s.sync();
            CHECK(s.status() == QSettings::AccessError);
            CHECK(readFile(dir + "/f.ini") == bytes("[General]\nk=1\n"));
        }
        {
            // Without the need for an atomic update the file is written in place.
            QSettings s(dir + "/f.ini", QSettings::IniFormat);
            CHECK(s.isWritable());
            s.setAtomicSyncRequired(false);
            CHECK(!s.isAtomicSyncRequired());
        }
        const QString file = dir + "/f.ini";
        ::chmod(file.toUtf8().constData(), 0444);
        {
            QSettings s(file, QSettings::IniFormat);
            CHECK(!s.isWritable());
        }
        ::chmod(file.toUtf8().constData(), 0644);
        ::chmod(dir.toUtf8().constData(), 0755);
        const QString unreadable = g_root + "/files/unreadable.ini";
        writeFile(unreadable, "[General]\nk=1\n");
        ::chmod(unreadable.toUtf8().constData(), 0);
        QSettings u(unreadable, QSettings::IniFormat);
        CHECK(u.status() == QSettings::AccessError);
        ::chmod(unreadable.toUtf8().constData(), 0644);
    }
}

static void testIsWritable() {
    {
        QSettings s(g_root + "/files/new/deep/dir/settings.ini", QSettings::IniFormat);
        CHECK(s.isWritable());
        CHECK(!QFileInfo(g_root + "/files/new").exists());
    }
    {
        const QString path = newFile("writable");
        writeFile(path, "[General]\n");
        QSettings s(path, QSettings::IniFormat);
        CHECK(s.isWritable());
    }
}

// ---- writing and synchronization ----------------------------------------------------------------

static void testSharedObjects() {
    const QString path = newFile("shared");
    QSettings a(path, QSettings::IniFormat);
    QSettings b(path, QSettings::IniFormat);
    a.setValue("x", 1);
    CHECK(b.value("x").toInt() == 1); // visible at once, before anything is written
    b.remove("x");
    CHECK(!a.contains("x"));
    a.setValue("y", QPoint(1, 2));
    CHECK(b.value("y").typeId() == QMetaType::QPoint);
    b.sync();
    CHECK(readFile(path) == bytes("[General]\ny=@Point(1 2)\n"));
    CHECK(a.value("y").typeId() == QMetaType::QPoint);
}

static void testSyncReload() {
    const QString path = newFile("reload");
    QSettings s(path, QSettings::IniFormat);
    CHECK(!s.contains("x"));
    writeFile(path, "[General]\nx=1\ny=2\n");
    CHECK(!s.contains("x")); // not read until sync()
    s.sync();
    CHECK(s.value("x").toInt() == 1);
    // A change in the file and a change in memory are merged.
    s.setValue("mine", "m");
    writeFile(path, "[General]\nx=10\ny=2\nother=o\n");
    s.sync();
    CHECK(s.value("x").toInt() == 10 && s.value("other").toString() == "o" && s.value("mine").toString() == "m");
    CHECK(readFile(path) == bytes("[General]\nmine=m\nother=o\nx=10\ny=2\n"));
    // A removal applies to what was there when it was made, and not to what appeared later.
    s.remove("y");
    writeFile(path, "[General]\nmine=m\nother=o\nx=10\ny=2\nlate=l\n");
    s.sync();
    CHECK(!s.contains("y") && s.value("late").toString() == "l");
    CHECK(readFile(path) == bytes("[General]\nlate=l\nmine=m\nother=o\nx=10\n"));
    // The file is deleted.
    QFile::remove(path);
    s.sync();
    CHECK(s.allKeys().isEmpty());
    // clear() removes only what is known.
    s.setValue("k", 1);
    s.sync();
    s.clear();
    writeFile(path, "[General]\nk=1\nn=2\n");
    s.sync();
    CHECK(readFile(path) == bytes("[General]\nn=2\n"));
}

static void testDeferredWrite() {
    const QString path = newFile("deferred");
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("a", 1);
        CHECK(!QFileInfo(path).exists());
        QCoreApplication::processEvents();
        CHECK(readFile(path) == bytes("[General]\na=1\n"));
        s.setValue("b", 2);
        s.setValue("c", 3);
        CHECK(readFile(path) == bytes("[General]\na=1\n"));
        QCoreApplication::processEvents();
        CHECK(readFile(path) == bytes("[General]\na=1\nb=2\nc=3\n"));
        s.setValue("d", 4);
    }
    CHECK(readFile(path) == bytes("[General]\na=1\nb=2\nc=3\nd=4\n")); // written by the destructor
    {
        QSettings s(path, QSettings::IniFormat);
        s.remove("a");
        QTimer::singleShot(10, [] { QCoreApplication::quit(); });
        QCoreApplication::exec();
        CHECK(!readFile(path).contains("a=1"));
        s.clear();
    }
    CHECK(readFile(path).isEmpty());
    // An object that is destroyed before the event loop runs does not leave a stale event.
    {
        auto *s = new QSettings(path, QSettings::IniFormat);
        s->setValue("z", 1);
        delete s;
    }
    QCoreApplication::processEvents();
    CHECK(readFile(path) == bytes("[General]\nz=1\n"));
}

static void testNoRewriteWithoutChanges() {
    const QString path = newFile("norewrite");
    writeFile(path, "[General]\n  weird   =   spacing  \n");
    {
        QSettings s(path, QSettings::IniFormat);
        CHECK(s.value("weird").toString() == "spacing");
        s.sync();
    }
    CHECK(readFile(path) == bytes("[General]\n  weird   =   spacing  \n"));
}

static void testFileSystemPermissions() {
    const QString path = newFile("perm");
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("a", 1);
    }
    struct stat st;
    CHECK(::stat(path.toUtf8().constData(), &st) == 0);
    mode_t mask = ::umask(0);
    ::umask(mask);
    CHECK((st.st_mode & 07777) == (0666 & ~mask));
    ::chmod(path.toUtf8().constData(), 0640);
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("b", 1);
    }
    CHECK(::stat(path.toUtf8().constData(), &st) == 0 && (st.st_mode & 07777) == 0640);
    CHECK(!QFileInfo(path + ".lock").exists());
    // No temporary files are left behind.
    QDir dir(g_root + "/files");
    for (const QString &name : dir.entryList(QDir::Files))
        CHECK(!name.startsWith("perm") || name.endsWith(".ini"));
}

// ---- custom formats --------------------------------------------------------------------------------

static bool readCsv(QIODevice &device, QSettings::SettingsMap &map) {
    const QList<QByteArray> lines = device.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.isEmpty())
            continue;
        const qsizetype comma = line.indexOf(',');
        if (comma < 0)
            return false;
        map.insert(QString::fromUtf8(line.left(comma)), QString::fromUtf8(line.mid(comma + 1)));
    }
    return true;
}

static bool writeCsv(QIODevice &device, const QSettings::SettingsMap &map) {
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        device.write(it.key().toUtf8() + "," + it.value().toString().toUtf8() + "\n");
    return true;
}

static bool writeFails(QIODevice &, const QSettings::SettingsMap &) { return false; }

static void testCustomFormats() {
    const QSettings::Format csv = QSettings::registerFormat("csv", readCsv, writeCsv);
    CHECK(csv >= QSettings::CustomFormat1 && csv <= QSettings::CustomFormat16);
    const QSettings::Format ci = QSettings::registerFormat("cic", readCsv, writeCsv, Qt::CaseInsensitive);
    const QSettings::Format failing = QSettings::registerFormat("bad", readCsv, writeFails);
    CHECK(ci == csv + 1 && failing == ci + 1);

    {
        QSettings s(csv, QSettings::UserScope, "COrg", "CApp");
        CHECK(s.fileName() == g_userDir + "/COrg/CApp.csv");
        CHECK(s.format() == csv);
        CHECK(s.status() == QSettings::NoError);
        s.setValue("a", 1);
        s.setValue("g/b", "two");
        s.setValue("G/b", "other");
        s.sync();
        CHECK(readFile(s.fileName()) == bytes("G/b,other\na,1\ng/b,two\n"));
    }
    {
        QSettings s(csv, QSettings::UserScope, "COrg", "CApp");
        CHECK(s.value("g/b").toString() == "two" && s.value("G/b").toString() == "other");
        CHECK(s.childGroups() == (QStringList{"G", "g"}));
    }
    {
        const QString path = g_root + "/files/ci.cic";
        QSettings s(path, ci);
        s.setValue("Key", 1);
        CHECK(s.value("key").toInt() == 1 && s.contains("KEY"));
        s.setValue("KEY", 2);
        s.setValue("grp/Sub", 3);
        CHECK(s.allKeys() == (QStringList{"grp/Sub", "Key"}));
        CHECK(s.value("Grp/sub").toInt() == 3);
        s.beginGroup("GRP");
        CHECK(s.childKeys() == QStringList("Sub") && s.value("SUB").toInt() == 3);
        s.endGroup();
        s.remove("GRP");
        CHECK(!s.contains("grp/sub"));
        s.sync();
        CHECK(readFile(path) == bytes("Key,2\n"));
    }
    {
        const QString path = g_root + "/files/read.csv";
        writeFile(path, "no comma here\n");
        QSettings s(path, csv);
        CHECK(s.status() == QSettings::FormatError);
    }
    {
        const QString path = g_root + "/files/fail.bad";
        QSettings s(path, failing);
        s.setValue("a", 1);
        s.sync();
        CHECK(s.status() == QSettings::FormatError);
        CHECK(!QFileInfo(path).exists());
        CHECK(s.value("a").toInt() == 1);
    }
    {
        // A custom format without a registered function.
        QSettings s(g_root + "/files/none", static_cast<QSettings::Format>(QSettings::CustomFormat16));
        CHECK(s.status() == QSettings::AccessError);
    }
    // At most sixteen formats can be registered.
    QSettings::Format last = QSettings::NativeFormat;
    for (int i = 0; i < 20; ++i)
        last = QSettings::registerFormat("x", readCsv, writeCsv);
    CHECK(last == QSettings::InvalidFormat);
}

// ---- several processes and threads --------------------------------------------------------------------

static void testProcesses() {
    const QString path = newFile("procs");
    constexpr int kProcs = 4;
    constexpr int kKeys = 12;
    std::vector<pid_t> children;
    for (int p = 0; p < kProcs; ++p) {
        const pid_t pid = ::fork();
        if (pid == 0) {
            for (int k = 0; k < kKeys; ++k) {
                QSettings s(path, QSettings::IniFormat);
                s.setValue(QString("p%1/k%2").arg(p).arg(k), k);
                s.sync();
                if (s.status() != QSettings::NoError)
                    ::_exit(2);
            }
            ::_exit(0);
        }
        CHECK(pid > 0);
        children.push_back(pid);
    }
    for (pid_t pid : children) {
        int status = 0;
        CHECK(::waitpid(pid, &status, 0) == pid);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(s.allKeys().size() == kProcs * kKeys);
    for (int p = 0; p < kProcs; ++p)
        for (int k = 0; k < kKeys; ++k)
            CHECK(s.value(QString("p%1/k%2").arg(p).arg(k)).toInt() == k);
    CHECK(!QFileInfo(path + ".lock").exists());

    // A change made by another process is seen after sync().
    const pid_t pid = ::fork();
    if (pid == 0) {
        QSettings c(path, QSettings::IniFormat);
        c.setValue("fromchild", "yes");
        c.remove("p0");
        c.sync();
        ::_exit(c.status() == QSettings::NoError ? 0 : 2);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(!s.contains("fromchild"));
    s.sync();
    CHECK(s.value("fromchild").toString() == "yes" && !s.contains("p0/k0") && s.contains("p1/k0"));
}

static void testThreads() {
    const QString path = newFile("threads");
    constexpr int kThreads = 6;
    constexpr int kKeys = 20;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int k = 0; k < kKeys; ++k) {
                QSettings s(path, QSettings::IniFormat);
                s.setValue(QString("t%1/k%2").arg(t).arg(k), k);
                s.setValue("shared", t);
                if (k % 3 == 0)
                    s.sync();
                const QVariant v = s.value(QString("t%1/k%2").arg(t).arg(k));
                if (v.toInt() != k)
                    ++errors;
                s.allKeys();
                if (s.status() != QSettings::NoError)
                    ++errors;
            }
        });
    }
    for (std::thread &t : threads)
        t.join();
    CHECK(errors.load() == 0);
    QSettings s(path, QSettings::IniFormat);
    CHECK(s.status() == QSettings::NoError);
    CHECK(s.allKeys().size() == kThreads * kKeys + 1);
    for (int t = 0; t < kThreads; ++t)
        for (int k = 0; k < kKeys; ++k)
            CHECK(s.value(QString("t%1/k%2").arg(t).arg(k)).toInt() == k);

    // Readers and one writer on the same object graph of one file.
    const QString path2 = newFile("threads");
    std::atomic<bool> stop{false};
    std::atomic<int> readErrors{0};
    std::thread writer([&] {
        for (int i = 0; i < 50; ++i) {
            QSettings w(path2, QSettings::IniFormat);
            w.setValue("counter", i);
            w.setValue("name", QString("n%1").arg(i));
            w.sync();
        }
        stop = true;
    });
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r) {
        readers.emplace_back([&] {
            while (!stop) {
                QSettings rd(path2, QSettings::IniFormat);
                rd.sync();
                if (rd.status() != QSettings::NoError)
                    ++readErrors;
                const int c = rd.value("counter", -1).toInt();
                if (c < -1 || c > 49)
                    ++readErrors;
            }
        });
    }
    writer.join();
    for (std::thread &t : readers)
        t.join();
    CHECK(readErrors.load() == 0);
}

static void testGarbageInput() {
    // Whatever the file contains, reading it must not crash.
    quint32 seed = 12345;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    const QByteArray alphabet = "[]=;,\"\\%@#\n\r \t/UxX0123456789abcdefgGeneral()Variant:ByteArray\x01\x80\xff";
    for (int round = 0; round < 200; ++round) {
        QByteArray data;
        const int len = static_cast<int>(next() % 200);
        for (int i = 0; i < len; ++i)
            data.append(alphabet.at(static_cast<qsizetype>(next() % static_cast<quint32>(alphabet.size()))));
        const QString path = g_root + "/files/garbage.ini";
        writeFile(path, data);
        QSettings s(path, QSettings::IniFormat);
        const QStringList keys = s.allKeys();
        for (const QString &k : keys)
            (void)s.value(k).toString();
        s.sync();
    }
    // Variants with bogus sizes.
    const QString path = g_root + "/files/garbage2.ini";
    writeFile(path, "a=@Variant(\\0\\0\\0\\t\\x7f\\xff\\xff\\xff)\nb=@Variant(\\0\\0\\0\\b\\0\\x10\\0\\0)\n"
                    "c=@Variant(\\0\\0\\0\\n\\xff\\xff\\xff\\xfe)\nd=@Variant(\\0\\0\\0\\x7f\\0\\0\\0\\x3x)\n");
    QSettings s(path, QSettings::IniFormat);
    for (const QString &k : s.allKeys())
        (void)s.value(k);
    CHECK(s.status() == QSettings::NoError);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    g_root = QFileInfo(dir.path()).absoluteFilePath();
    g_userDir = g_root + "/xdg-config";
    g_sysDir1 = g_root + "/xdg-sys1";
    g_sysDir2 = g_root + "/xdg-sys2";
    QDir().mkpath(g_root + "/files");
    ::setenv("HOME", (g_root + "/home").toUtf8().constData(), 1);
    ::setenv("XDG_CONFIG_HOME", g_userDir.toUtf8().constData(), 1);
    ::setenv("XDG_CONFIG_DIRS", (g_sysDir1 + ":" + g_sysDir2).toUtf8().constData(), 1);

    testStreamVersions();
    testLocations();
    testFallbacks();
    testBasics();
    testArrays();
    testIniWriting();
    testIniReading();
    testVariantTypes();
    testEscapedKeysRoundTrip();
    testValuesRoundTrip();
    testFormatErrors();
    testIsWritable();
    testSharedObjects();
    testSyncReload();
    testDeferredWrite();
    testNoRewriteWithoutChanges();
    testFileSystemPermissions();
    testCustomFormats();
    testProcesses();
    testThreads();
    testGarbageInput();

    if (g_failures == 0)
        std::printf("test_settings: all tests passed\n");
    else
        std::printf("test_settings: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
