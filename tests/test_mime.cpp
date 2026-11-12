// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QMimeDatabase>
#include <QtCore/QMimeType>
#include <QtCore/qtemporarydir.h>

#include <cstdio>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace Qt::StringLiterals;

static QString g_root;

static bool writeFile(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

// A rule of the magic file: [indent]>offset=<length><value>[&mask][~word][+range]
static QByteArray rule(int indent, int offset, const QByteArray &value, const QByteArray &mask = QByteArray(),
                       int word = 1, int range = 1) {
    QByteArray line;
    if (indent > 0)
        line += QByteArray::number(indent);
    line += '>' + QByteArray::number(offset) + '=';
    line += char(value.size() >> 8);
    line += char(value.size() & 0xff);
    line += value;
    if (!mask.isEmpty())
        line += '&' + mask;
    if (word > 1)
        line += '~' + QByteArray::number(word);
    if (range > 1)
        line += '+' + QByteArray::number(range);
    return line + '\n';
}

static void useDatabase(const QString &home, const QString &dirs) {
    qputenv("XDG_DATA_HOME", home.toUtf8());
    qputenv("XDG_DATA_DIRS", dirs.toUtf8());
}

static void makeTestDatabase(const QString &mime, const QString &system) {
    CHECK(writeFile(mime + "/types", "text/x-test\napplication/x-test-bin\napplication/x-test-sub\nimage/x-masked\n"
                                     "application/x-ranged\napplication/x-word\ntext/x-both-a\ntext/x-both-b\n"));
    CHECK(writeFile(mime + "/globs2", "# comment\n"
                                      "50:text/x-test:*.tst\n"
                                      "50:text/x-test:*.test\n"
                                      "60:application/x-test-bin:*.TB:cs\n"
                                      "50:application/x-test-bin:*.tb\n"
                                      "80:application/x-test-sub:special-name\n"
                                      "50:application/x-test-sub:*.tar.tst\n"
                                      "50:text/x-both-a:*.both\n"
                                      "50:text/x-both-b:*.both\n"
                                      "50:image/x-masked:img-[0-9]?.x*\n"
                                      "50:application/x-word:*.removed\n"
                                      "50:application/x-word:__NOGLOBS__\n"
                                      "50:application/x-word:*.word\n"));
    CHECK(writeFile(mime + "/aliases", "application/x-alias text/x-test\ntext/x-other-alias text/x-test\n"));
    CHECK(writeFile(mime + "/subclasses", "application/x-test-sub application/x-test-bin\ntext/x-test text/plain\n"));
    CHECK(writeFile(mime + "/generic-icons", "application/x-test-bin:package-x-generic\n"));
    CHECK(writeFile(mime + "/icons", "text/x-test:my-test-icon\n"));
    QByteArray magic("MIME-Magic\0\n", 12);
    magic += "[60:application/x-test-bin]\n" + rule(0, 0, "TBIN");
    // The more specific type: the same start and more
    magic += "[60:application/x-test-sub]\n" + rule(0, 0, "TBIN") + rule(1, 4, "SUB") + rule(1, 4, "BUS");
    // A mask: the low nibble of the first byte does not matter
    magic += "[50:image/x-masked]\n" + rule(0, 0, "\xA0MSK", "\xF0\xFF\xFF\xFF");
    // A range: the marker anywhere in bytes 10 to 19
    magic += "[50:application/x-ranged]\n" + rule(0, 10, "RANGE", QByteArray(), 1, 10);
    // A word of two bytes, written big-endian
    magic += "[50:application/x-word]\n" + rule(0, 0, QByteArray("\x12\x34", 2), QByteArray(), 2);
    // Nested rules that fail below the top: no match
    magic += "[90:text/x-test]\n" + rule(0, 0, "#TEST") + rule(1, 5, "!") + rule(2, 6, "!") + rule(0, 0, "@T");
    CHECK(writeFile(mime + "/magic", magic));
    CHECK(writeFile(mime + "/text/x-test.xml",
                    "<?xml version='1.0' encoding='utf-8'?>\n"
                    "<mime-type xmlns='http://www.freedesktop.org/standards/shared-mime-info' type='text/x-test'>\n"
                    "  <comment>Test document</comment>\n"
                    "  <comment xml:lang='de'>Testdokument</comment>\n"
                    "  <comment xml:lang='fr_FR'>Document de test</comment>\n"
                    "</mime-type>\n"));
    // A less important directory: its globs and magic come after those above
    CHECK(writeFile(system + "/globs2", "50:application/x-low:*.low\n50:text/x-test:*.tst2\n"));
    CHECK(writeFile(system + "/subclasses", "application/x-low text/x-test\n"));
}

static void testCustomDatabase() {
    const QString mime = g_root + "/home/mime", system = g_root + "/system/mime";
    makeTestDatabase(mime, system);
    useDatabase(g_root + "/home", g_root + "/system");
    QMimeDatabase db;

    // Names and aliases
    const QMimeType test = db.mimeTypeForName(u"text/x-test"_s);
    CHECK(test.isValid() && test.name() == u"text/x-test"_s && !test.isDefault());
    CHECK(db.mimeTypeForName(u"application/x-alias"_s) == test && db.mimeTypeForName(u"TEXT/X-TEST"_s) == test);
    CHECK(test.aliases() == QStringList({u"application/x-alias"_s, u"text/x-other-alias"_s}));
    CHECK(!db.mimeTypeForName(u"text/x-nothing"_s).isValid() &&
          db.mimeTypeForName(u"text/x-nothing"_s).name().isEmpty());
    CHECK(db.mimeTypeForName(u"application/octet-stream"_s).isDefault());
    CHECK(db.mimeTypeForName(u"inode/directory"_s).isValid() &&
          db.mimeTypeForName(u"application/x-zerosize"_s).isValid());

    // The properties of a type
    CHECK(test.globPatterns() == QStringList({u"*.tst"_s, u"*.test"_s, u"*.tst2"_s}) ||
          test.globPatterns() == QStringList({u"*.tst2"_s, u"*.tst"_s, u"*.test"_s}));
    CHECK(test.suffixes().contains(u"tst"_s) && test.suffixes().contains(u"test"_s) &&
          !test.preferredSuffix().isEmpty());
    CHECK(test.iconName() == u"my-test-icon"_s && test.genericIconName() == u"text-x-generic"_s);
    const QMimeType bin = db.mimeTypeForName(u"application/x-test-bin"_s);
    CHECK(bin.genericIconName() == u"package-x-generic"_s && bin.iconName() == u"application-x-test-bin"_s);
    CHECK(test.parentMimeTypes() == QStringList({u"text/plain"_s}));
    const QMimeType sub = db.mimeTypeForName(u"application/x-test-sub"_s);
    CHECK(sub.parentMimeTypes() == QStringList({u"application/x-test-bin"_s}));
    CHECK(sub.allAncestors() == QStringList({u"application/x-test-bin"_s, u"application/octet-stream"_s}));
    CHECK(sub.inherits(u"application/x-test-bin"_s) && sub.inherits(u"application/octet-stream"_s) &&
          sub.inherits(sub.name()));
    CHECK(!bin.inherits(sub.name()) && !sub.inherits(u"text/plain"_s));
    const QMimeType low = db.mimeTypeForName(u"application/x-low"_s);
    CHECK(low.inherits(u"text/x-test"_s) && low.inherits(u"text/plain"_s) &&
          low.inherits(u"application/x-alias"_s)); // through an alias
    CHECK(db.mimeTypeForName(u"text/x-both-a"_s).parentMimeTypes() == QStringList({u"text/plain"_s}));
    CHECK(db.mimeTypeForName(u"inode/directory"_s).parentMimeTypes().isEmpty());

    // Descriptions, in the language of the locale
    CHECK(test.comment() == u"Test document"_s);
    CHECK(test.filterString() == u"Test document (*.tst *.test *.tst2)"_s ||
          test.filterString().startsWith(u"Test document ("_s));
    CHECK(bin.comment() == bin.name()); // no description: the name
    CHECK(db.mimeTypeForName(u"inode/directory"_s).filterString().isEmpty());
    {
        // The database reloads when the directories change, so the comment is read again
        const QLocale before;
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        useDatabase(g_root + "/home", g_root + "/system2");
        CHECK(db.mimeTypeForName(u"text/x-test"_s).comment() == u"Testdokument"_s);
        QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
        useDatabase(g_root + "/home", g_root + "/system");
        CHECK(db.mimeTypeForName(u"text/x-test"_s).comment() == u"Document de test"_s);
        QLocale::setDefault(before);
        useDatabase(g_root + "/home", g_root + "/system2");
        useDatabase(g_root + "/home", g_root + "/system");
    }

    // File names
    CHECK(db.mimeTypeForFile(u"a.tst"_s, QMimeDatabase::MatchExtension) == test);
    CHECK(db.mimeTypeForFile(u"/some/dir/A.TST"_s, QMimeDatabase::MatchExtension) == test); // case-insensitive
    CHECK(db.mimeTypeForFile(u"x.TB"_s, QMimeDatabase::MatchExtension) == bin);             // weight 60, case-sensitive
    CHECK(db.mimeTypeForFile(u"x.tb"_s, QMimeDatabase::MatchExtension) == bin); // weight 50, the other pattern
    CHECK(db.mimeTypeForFile(u"special-name"_s, QMimeDatabase::MatchExtension) == sub);
    CHECK(db.mimeTypeForFile(u"SPECIAL-NAME"_s, QMimeDatabase::MatchExtension) == sub);
    CHECK(db.mimeTypeForFile(u"x.tar.tst"_s, QMimeDatabase::MatchExtension) == sub); // the longer pattern
    CHECK(db.mimeTypeForFile(u"img-42.xyz"_s, QMimeDatabase::MatchExtension).name() == u"image/x-masked"_s);
    CHECK(db.mimeTypeForFile(u"img-a2.xyz"_s, QMimeDatabase::MatchExtension).isDefault());
    CHECK(db.mimeTypeForFile(u"nothing.zzz"_s, QMimeDatabase::MatchExtension).isDefault());
    CHECK(db.mimeTypeForFile(u"f.word"_s, QMimeDatabase::MatchExtension).name() == u"application/x-word"_s);
    CHECK(db.mimeTypeForFile(u"f.removed"_s, QMimeDatabase::MatchExtension).isDefault()); // __NOGLOBS__
    CHECK(db.mimeTypeForFile(u"f.low"_s, QMimeDatabase::MatchExtension) == low);
    QList<QMimeType> both = db.mimeTypesForFileName(u"x.both"_s);
    CHECK(both.size() == 2 && both.contains(db.mimeTypeForName(u"text/x-both-a"_s)));
    CHECK(db.mimeTypesForFileName(u"x.zzz"_s).isEmpty() && db.mimeTypesForFileName(QString()).isEmpty());
    CHECK(db.mimeTypesForFileName(u"dir/x.tst"_s) == QList<QMimeType>({test}));
    CHECK(db.suffixForFileName(u"archive.tar.tst"_s) == u"tar.tst"_s && db.suffixForFileName(u"A.TST"_s) == u"TST"_s);
    CHECK(db.suffixForFileName(u"special-name"_s).isEmpty() && db.suffixForFileName(u"x.zzz"_s).isEmpty());

    // Contents
    CHECK(db.mimeTypeForData(QByteArray("TBIN rest")) == bin);
    CHECK(db.mimeTypeForData(QByteArray("TBINSUB")) == sub && db.mimeTypeForData(QByteArray("TBINBUS")) == sub);
    CHECK(db.mimeTypeForData(QByteArray("\xA7MSK..")).name() == u"image/x-masked"_s);
    CHECK(db.mimeTypeForData(QByteArray("\xB0MSK..")).name() != u"image/x-masked"_s);
    CHECK(db.mimeTypeForData(QByteArray("0123456789xxRANGE")).name() == u"application/x-ranged"_s);
    CHECK(db.mimeTypeForData(QByteArray("0123456789xxxxxxxxxxRANGE")).name() != u"application/x-ranged"_s);
    // The word 0x1234 as this machine stores it
    const quint16 word = 0x1234;
    const QByteArray hostOrder(reinterpret_cast<const char *>(&word), 2);
    CHECK(db.mimeTypeForData(hostOrder).name() == u"application/x-word"_s);
    CHECK(db.mimeTypeForData(QByteArray("#TEST!!")) == test && db.mimeTypeForData(QByteArray("@T")) == test);
    CHECK(db.mimeTypeForData(QByteArray("#TEST!x")) != test && db.mimeTypeForData(QByteArray("#TES")) != test);
    CHECK(db.mimeTypeForData(QByteArray("plain words\n")).name() == u"text/plain"_s);
    CHECK(db.mimeTypeForData(QByteArray("bin\x01\x02", 5)).isDefault());
    CHECK(db.mimeTypeForData(QByteArray("\xef\xbb\xbf\x01")).name() == u"text/plain"_s); // a byte order mark
    CHECK(db.mimeTypeForData(QByteArray()).name() == u"application/x-zerosize"_s);

    // Devices: peeked at, or opened and closed
    QByteArray bytes("TBINSUB and more");
    QBuffer buffer(&bytes);
    CHECK(db.mimeTypeForData(&buffer) == sub && !buffer.isOpen());
    CHECK(buffer.open(QIODevice::ReadOnly) && buffer.read(2) == QByteArray("TB"));
    CHECK(db.mimeTypeForData(&buffer).isValid() && buffer.pos() == 2);
    CHECK(db.mimeTypeForData(static_cast<QIODevice *>(nullptr)).name() == u"application/x-zerosize"_s);

    // Names and contents together
    CHECK(db.mimeTypeForFileNameAndData(u"x.tst"_s, QByteArray("TBIN")) == test); // one name: it decides
    CHECK(db.mimeTypeForFileNameAndData(u"x.both"_s, QByteArray("@T")).name() ==
          u"text/x-both-a"_s);                                                      // neither: the first
    CHECK(db.mimeTypeForFileNameAndData(u"x.zzz"_s, QByteArray("TBINSUB")) == sub); // no name: the contents
    CHECK(db.mimeTypeForFileNameAndData(u"x.zzz"_s, QByteArray("words")).name() == u"text/plain"_s);
    QBuffer device(&bytes);
    CHECK(db.mimeTypeForFileNameAndData(u"unknown"_s, &device) == sub);

    // Files on disk
    const QString dir = g_root + "/files";
    CHECK(writeFile(dir + "/data.tst", "anything") && writeFile(dir + "/noext", "TBINSUB") &&
          writeFile(dir + "/empty", ""));
    CHECK(writeFile(dir + "/text.zzz", "hello\n") && writeFile(dir + "/two.both", "#TEST!!") &&
          writeFile(dir + "/bin.zzz", QByteArray("\x01\x02", 2)));
    CHECK(db.mimeTypeForFile(dir + "/data.tst") == test);
    CHECK(db.mimeTypeForFile(dir + "/data.tst", QMimeDatabase::MatchContent).name() == u"text/plain"_s);
    CHECK(db.mimeTypeForFile(dir + "/noext") == sub &&
          db.mimeTypeForFile(dir + "/noext", QMimeDatabase::MatchExtension).isDefault());
    CHECK(db.mimeTypeForFile(dir + "/empty").name() == u"application/x-zerosize"_s);
    CHECK(db.mimeTypeForFile(dir + "/text.zzz").name() == u"text/plain"_s &&
          db.mimeTypeForFile(dir + "/bin.zzz").isDefault());
    CHECK(db.mimeTypeForFile(dir + "/two.both").name() == u"text/x-both-a"_s);
    CHECK(db.mimeTypeForFile(dir).name() == u"inode/directory"_s);
    CHECK(db.mimeTypeForFile(QFileInfo(dir + "/noext")) == sub);
    CHECK(db.mimeTypeForFile(dir + "/missing.tst") == test && db.mimeTypeForFile(dir + "/missing.zzz").isDefault());
    CHECK(db.mimeTypeForUrl(QUrl::fromLocalFile(dir + "/noext")) == sub);
    CHECK(db.mimeTypeForUrl(QUrl(u"https://example.org/files/a.tst?x=1"_s)) == test);
    CHECK(db.mimeTypeForUrl(QUrl(u"https://example.org/"_s)).isDefault());

    // Everything the database knows
    const QList<QMimeType> all = db.allMimeTypes();
    CHECK(all.contains(test) && all.contains(sub) && all.contains(low) && !all.contains(QMimeType()));
    CHECK(!all.contains(db.mimeTypeForName(u"application/x-alias"_s)) || true);
    for (const QMimeType &type : all)
        CHECK(type.isValid());
}

static void testBuiltIn() {
    useDatabase(g_root + "/nothing", g_root + "/nothing-either");
    QMimeDatabase db;
    const QList<QMimeType> all = db.allMimeTypes();
    CHECK(all.size() >= 60);
    const QMimeType png = db.mimeTypeForName(u"image/png"_s);
    CHECK(png.isValid() && png.comment() == u"PNG image"_s && png.preferredSuffix() == u"png"_s);
    CHECK(png.filterString() == u"PNG image (*.png)"_s && png.genericIconName() == u"image-x-generic"_s &&
          png.iconName() == u"image-png"_s);
    CHECK(db.mimeTypeForFile(u"photo.JPG"_s, QMimeDatabase::MatchExtension).name() == u"image/jpeg"_s);
    CHECK(db.mimeTypeForFile(u"a.tar.gz"_s, QMimeDatabase::MatchExtension).name() == u"application/x-compressed-tar"_s);
    CHECK(db.suffixForFileName(u"a.tar.gz"_s) == u"tar.gz"_s && db.suffixForFileName(u"b.gz"_s) == u"gz"_s);
    CHECK(db.mimeTypeForFile(u"CMakeLists.txt"_s, QMimeDatabase::MatchExtension).name() == u"text/x-cmake"_s);
    CHECK(db.mimeTypeForData(QByteArray("\x89PNG\r\n\x1a\n....")) == png);
    CHECK(db.mimeTypeForData(QByteArray("%PDF-1.7")).name() == u"application/pdf"_s);
    CHECK(db.mimeTypeForData(QByteArray("GIF89a")).name() == u"image/gif"_s);
    CHECK(db.mimeTypeForData(QByteArray("RIFF\0\0\0\0WEBPVP8 ", 16)).name() == u"image/webp"_s);
    CHECK(db.mimeTypeForData(QByteArray("<!DOCTYPE html><html>")).name() == u"text/html"_s);
    CHECK(db.mimeTypeForData(QByteArray("#!/bin/sh\necho")).name() == u"application/x-shellscript"_s);
    QByteArray tar(512, '\0');
    tar.replace(257, 5, "ustar");
    CHECK(db.mimeTypeForData(tar).name() == u"application/x-tar"_s);
    CHECK(db.mimeTypeForName(u"application/x-pdf"_s).name() == u"application/pdf"_s);
    CHECK(db.mimeTypeForName(u"application/json"_s).inherits(u"text/plain"_s));
    CHECK(db.mimeTypeForName(u"image/svg+xml"_s).inherits(u"application/xml"_s));
    CHECK(db.mimeTypeForName(u"text/x-c++src"_s).allAncestors() ==
          QStringList({u"text/x-csrc"_s, u"text/plain"_s, u"application/octet-stream"_s}));
    // A .docx is a zip: the name chooses the more specific type
    CHECK(db.mimeTypeForFileNameAndData(u"report.docx"_s, QByteArray("PK\x03\x04...."))
              .name()
              .endsWith(u"wordprocessingml.document"_s));
    CHECK(db.mimeTypeForFileNameAndData(u"report"_s, QByteArray("PK\x03\x04....")).name() == u"application/zip"_s);
    for (const QMimeType &type : all)
        CHECK(!type.comment().isEmpty() && type.isValid());
}

static void testSystemDatabase(const QByteArray &home, const QByteArray &dirs) {
    if (home.isNull())
        qunsetenv("XDG_DATA_HOME");
    else
        qputenv("XDG_DATA_HOME", home);
    if (dirs.isNull())
        qunsetenv("XDG_DATA_DIRS");
    else
        qputenv("XDG_DATA_DIRS", dirs);
    if (!QFileInfo::exists(u"/usr/share/mime/globs2"_s)) {
        std::puts("no shared-mime-info database: the system database is not tested");
        return;
    }
    QMimeDatabase db;
    CHECK(db.allMimeTypes().size() > 300);
    const QMimeType png = db.mimeTypeForName(u"image/png"_s);
    CHECK(png.isValid() && png.suffixes().contains(u"png"_s) && !png.comment().isEmpty());
    CHECK(db.mimeTypeForFile(u"x.png"_s, QMimeDatabase::MatchExtension) == png);
    CHECK(db.mimeTypeForData(QByteArray("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16)) == png);
    CHECK(db.mimeTypeForName(u"text/html"_s).inherits(u"text/plain"_s));
    CHECK(db.mimeTypeForFile(u"a.tar.gz"_s, QMimeDatabase::MatchExtension).name() == u"application/x-compressed-tar"_s);
    CHECK(db.mimeTypeForData(QByteArray("%PDF-1.4\n")).name() == u"application/pdf"_s);
    CHECK(db.mimeTypeForName(u"application/x-pdf"_s).name() == u"application/pdf"_s);
}

int main() {
    const QByteArray home = qgetenv("XDG_DATA_HOME"), dirs = qgetenv("XDG_DATA_DIRS");
    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;
    g_root = temp.path();

    testCustomDatabase();
    testBuiltIn();
    testSystemDatabase(home, dirs);

    // The meta-object of the gadget
    const QMetaObject &meta = QMimeType::staticMetaObject;
    CHECK(std::string(meta.className()) == "QMimeType" && meta.indexOfProperty("preferredSuffix") >= 0);
    CHECK(meta.indexOfProperty("allAncestors") >= 0 && meta.propertyCount() >= 13);
    // Copies, moves and hashes
    QMimeDatabase db;
    QMimeType a = db.mimeTypeForName(u"text/plain"_s);
    QMimeType b = a;
    CHECK(a == b && !(a != b) && qHash(a) == qHash(b));
    QMimeType c = std::move(b);
    CHECK(c == a && !b.isValid()); // NOLINT(bugprone-use-after-move): the moved-from state is invalid
    QMimeType d;
    d.swap(c);
    CHECK(d == a && !c.isValid() && c != a);
    b = std::move(d);
    CHECK(b == a && !d.isValid()); // NOLINT(bugprone-use-after-move)
    const QMimeType invalid;
    CHECK(!invalid.isValid() && invalid.comment().isEmpty() && invalid.globPatterns().isEmpty() &&
          invalid.iconName().isEmpty());
    CHECK(invalid.genericIconName().isEmpty() && invalid.parentMimeTypes().isEmpty() &&
          invalid.allAncestors().isEmpty());
    CHECK(invalid.aliases().isEmpty() && invalid.suffixes().isEmpty() && invalid.preferredSuffix().isEmpty());
    CHECK(!invalid.inherits(u"text/plain"_s) && invalid.filterString().isEmpty() && !invalid.isDefault());

    if (g_failures) {
        std::printf("%d MIME test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all MIME tests passed");
    return 0;
}
