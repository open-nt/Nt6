// MIT License - see LICENSE
//
// Tests of the resource system: ntrcc, QResource, and resource paths in QFile, QFileInfo, QDir and
// QDirIterator. The resources come from tests/resources (see CMakeLists.txt).
#include <QtCore>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static QString g_tmp;

static QByteArray readAll(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray("<cannot open>");
    return f.readAll();
}

static bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

// Exposes the protected members of QResource.
class ResourceProbe : public QResource {
public:
    explicit ResourceProbe(const QString &path) : QResource(path) {}
    using QResource::children;
    using QResource::isDir;
    using QResource::isFile;
};

static void testFile() {
    CHECK(QFile::exists(":/hello.txt"));
    CHECK(!QFile::exists(":/missing.txt"));
    CHECK(readAll(":/hello.txt") == "Hello, resources!\n");
    CHECK(readAll(":hello.txt") == "Hello, resources!\n");
    CHECK(readAll(":/data/../hello.txt") == "Hello, resources!\n");

    QFile f(":/renamed.txt");
    CHECK(f.size() == 19);
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.size() == 19);
    CHECK(f.handle() == -1);
    CHECK(f.readLine() == "first\n");
    CHECK(f.pos() == 6);
    CHECK(f.readLine() == "second\n");
    CHECK(f.readLine() == "third\n");
    CHECK(f.atEnd());
    CHECK(f.read(4).isEmpty());
    CHECK(f.seek(1));
    CHECK(f.read(4) == "irst");
    CHECK(f.seek(100));
    CHECK(f.read(1).isEmpty());
    CHECK(!f.resize(3));
    f.close();
    CHECK(!f.isOpen());

    QFile text(":/renamed.txt");
    CHECK(text.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&text);
    QStringList lines;
    while (!stream.atEnd())
        lines << stream.readLine();
    CHECK(lines == (QStringList() << "first" << "second" << "third"));

    QFile writable(":/hello.txt");
    CHECK(!writable.open(QIODevice::WriteOnly));
    CHECK(writable.error() == QFileDevice::OpenError);
    CHECK(!writable.open(QIODevice::ReadWrite));
    QFile missing(":/missing.txt");
    CHECK(!missing.open(QIODevice::ReadOnly));
    CHECK(missing.error() == QFileDevice::OpenError);
    QFile dir(":/icons");
    CHECK(!dir.open(QIODevice::ReadOnly));

    CHECK(!QFile::remove(":/hello.txt"));
    CHECK(!QFile::rename(":/hello.txt", g_tmp + "/renamed"));
    CHECK(!QFile::setPermissions(":/hello.txt", QFileDevice::ReadOwner));
    CHECK(QFile::exists(":/hello.txt"));
    const QFileDevice::Permissions readable =
        QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::ReadGroup | QFileDevice::ReadOther;
    CHECK(QFile::permissions(":/hello.txt") == readable);
    CHECK(QFile(":/hello.txt").permissions() == readable);
    CHECK(QFile::permissions(":/missing.txt") == QFileDevice::Permissions());
    CHECK(QFile(":/hello.txt").fileTime(QFileDevice::FileModificationTime).isValid());
    CHECK(!QFile(":/hello.txt").fileTime(QFileDevice::FileAccessTime).isValid());

    // Copying a resource out to a real file.
    const QString copy = g_tmp + "/copy.txt";
    CHECK(QFile::copy(":/hello.txt", copy));
    CHECK(readAll(copy) == "Hello, resources!\n");
}

static void testFileInfo() {
    QFileInfo fi(":/icons/open.svg");
    CHECK(fi.exists());
    CHECK(fi.isFile());
    CHECK(!fi.isDir());
    CHECK(!fi.isSymLink());
    CHECK(fi.isAbsolute());
    CHECK(!fi.isRelative());
    CHECK(fi.isReadable());
    CHECK(!fi.isWritable());
    CHECK(!fi.isExecutable());
    CHECK(fi.size() == 65);
    CHECK(fi.fileName() == "open.svg");
    CHECK(fi.suffix() == "svg");
    CHECK(fi.path() == ":/icons");
    CHECK(fi.absoluteFilePath() == ":/icons/open.svg");
    CHECK(fi.canonicalFilePath() == ":/icons/open.svg");
    CHECK(fi.absoluteDir().exists());
    CHECK(fi.lastModified().isValid());
    CHECK(!fi.lastRead().isValid());
    CHECK(fi.owner().isEmpty());
    CHECK(fi.ownerId() == static_cast<uint>(-2));
    CHECK(fi.permissions() == (QFileInfo::ReadOwner | QFileInfo::ReadUser | QFileInfo::ReadGroup |
                               QFileInfo::ReadOther));

    CHECK(QFileInfo(":/icons").isDir());
    CHECK(QFileInfo(":/icons/sub/").isDir());
    CHECK(QFileInfo(":/").isDir());
    CHECK(QFileInfo(":/").isRoot());
    CHECK(QFileInfo(":/").exists());
    CHECK(!QFileInfo(":/icons").isRoot());
    CHECK(!QFileInfo(":/nothing").exists());
    CHECK(QFileInfo(":/nothing").canonicalFilePath().isEmpty());
    CHECK(QFileInfo(":/text/a & b.txt").isFile());
    CHECK(QFileInfo::exists(":/text/greeting.txt"));
}

static void testDir() {
    CHECK(QDir::isAbsolutePath(":/x"));
    CHECK(QDir::cleanPath(":/a/../b/") == ":/b");
    CHECK(QDir::cleanPath(":") == ":/");
    CHECK(QDir::cleanPath(":/..") == ":/");
    CHECK(QDir::cleanPath(":a//b") == ":/a/b");

    QDir icons(":/icons");
    CHECK(icons.exists());
    CHECK(icons.isReadable());
    CHECK(icons.isAbsolute());
    CHECK(icons.dirName() == "icons");
    CHECK(icons.absolutePath() == ":/icons");
    CHECK(icons.entryList() == (QStringList() << "open.svg" << "sub"));
    CHECK(icons.entryList(QDir::Files) == QStringList("open.svg"));
    CHECK(icons.entryList(QDir::Dirs) == QStringList("sub"));
    CHECK(icons.entryList(QStringList("*.svg"), QDir::AllEntries) == QStringList("open.svg"));
    const QFileInfoList infos = icons.entryInfoList(QDir::Files);
    CHECK(infos.size() == 1 && infos.first().filePath() == ":/icons/open.svg");
    CHECK(icons.exists("open.svg"));
    CHECK(!icons.isEmpty());

    CHECK(icons.cd("sub"));
    CHECK(icons.path() == ":/icons/sub");
    CHECK(icons.entryList() == QStringList("close.svg"));
    CHECK(icons.cdUp());
    CHECK(icons.path() == ":/icons");
    CHECK(!icons.cd("missing"));

    CHECK(!icons.mkdir("new"));
    CHECK(!icons.mkpath("a/b"));
    CHECK(!icons.rmdir("sub"));
    CHECK(!icons.remove("open.svg"));
    CHECK(!icons.rename("open.svg", "x.svg"));
    CHECK(!QDir(":/icons").removeRecursively());
    CHECK(QFile::exists(":/icons/open.svg"));

    const QStringList top = QDir(":/").entryList();
    for (const char *name : {"auto", "hello.txt", "icons", "listed", "renamed.txt", "text"})
        CHECK(top.contains(QString::fromUtf8(name)));
    CHECK(!top.contains("."));
    CHECK(QDir(":").exists());
    CHECK(!QDir(":/hello.txt").exists());
    CHECK(!QDir(":/missing").exists());
    CHECK(QDir(":/missing").entryList().isEmpty());
}

static void testDirIterator() {
    QStringList found;
    QDirIterator it(":/icons", QDirIterator::Subdirectories);
    while (it.hasNext())
        found << it.next();
    found.sort();
    CHECK(found == (QStringList() << ":/icons/open.svg" << ":/icons/sub" << ":/icons/sub/close.svg"));

    QStringList files;
    QDirIterator svg(":/", QStringList("*.svg"), QDir::Files, QDirIterator::Subdirectories);
    while (svg.hasNext())
        files << svg.next();
    files.sort();
    CHECK(files == (QStringList() << ":/icons/open.svg" << ":/icons/sub/close.svg" << ":/listed/icons/open.svg"));
}

// QDir search paths, over resource and real directories.
static void testSearchPaths() {
    const QString disk = g_tmp + "/icons";
    CHECK(QDir().mkpath(disk + "/sub"));
    CHECK(writeFile(disk + "/extra.txt", "extra\n"));
    CHECK(writeFile(disk + "/open.svg", "on disk\n"));

    CHECK(QDir::searchPaths("icons").isEmpty());
    QDir::setSearchPaths("icons", QStringList() << ":/icons/" << disk);
    CHECK(QDir::searchPaths("icons") == (QStringList() << ":/icons" << disk));

    // The first search path that has the file wins.
    CHECK(QFile::exists("icons:open.svg"));
    CHECK(readAll("icons:open.svg").startsWith("<svg"));
    CHECK(readAll("icons:extra.txt") == "extra\n");
    CHECK(QFileInfo("icons:sub/close.svg").isFile());
    CHECK(QFileInfo("icons:sub/close.svg").absoluteFilePath() == ":/icons/sub/close.svg");
    CHECK(QFileInfo("icons:extra.txt").absoluteFilePath() == disk + "/extra.txt");
    CHECK(QFileInfo("icons:extra.txt").fileName() == "extra.txt");
    CHECK(QFileInfo("icons:extra.txt").filePath() == "icons:extra.txt");
    CHECK(QFileInfo("icons:extra.txt").size() == 6);
    CHECK(QFile("icons:extra.txt").size() == 6);
    CHECK(!QFile::exists("icons:missing.txt"));

    QDir dir("icons:sub");
    CHECK(dir.exists());
    CHECK(dir.absolutePath() == ":/icons/sub");
    CHECK(dir.entryList() == QStringList("close.svg"));
    CHECK(QFileInfo(dir.entryInfoList().value(0)).isFile());
    QStringList found;
    QDirIterator it("icons:sub");
    while (it.hasNext())
        found << it.next();
    CHECK(found == QStringList("icons:sub/close.svg"));

    // A file that exists nowhere is created in the first search path that is not read-only: here
    // the first is a resource, so writing fails; with the disk first, it is created there.
    QFile fresh("icons:new.txt");
    CHECK(!fresh.open(QIODevice::WriteOnly));
    QDir::setSearchPaths("icons", QStringList() << disk << ":/icons");
    CHECK(writeFile("icons:new.txt", "new\n"));
    CHECK(readAll(disk + "/new.txt") == "new\n");
    CHECK(readAll("icons:open.svg") == "on disk\n");
    CHECK(QFile::remove("icons:new.txt"));
    CHECK(!QFile::exists(disk + "/new.txt"));

    // addSearchPath() appends; a search path can use another prefix.
    QDir::setSearchPaths("theme", QStringList("icons:sub"));
    CHECK(readAll("theme:close.svg").startsWith("<svg"));
    QDir::addSearchPath("theme", ":/text");
    CHECK(QDir::searchPaths("theme") == (QStringList() << "icons:sub" << ":/text"));
    CHECK(QFile::exists("theme:greeting.txt"));

    // Prefixes that name each other resolve to nothing, without looping.
    QDir::setSearchPaths("loopa", QStringList("loopb:"));
    QDir::setSearchPaths("loopb", QStringList("loopa:"));
    CHECK(!QFile::exists("loopa:x"));
    CHECK(!QFileInfo("loopb:x").exists());

    // Invalid prefixes are refused, an empty list removes a prefix, and unknown prefixes are names.
    QDir::setSearchPaths("x", QStringList(disk));
    CHECK(QDir::searchPaths("x").isEmpty());
    QDir::addSearchPath("a-b", disk);
    CHECK(QDir::searchPaths("a-b").isEmpty());
    for (const char *prefix : {"icons", "theme", "loopa", "loopb"})
        QDir::setSearchPaths(QString::fromUtf8(prefix), QStringList());
    CHECK(QDir::searchPaths("icons").isEmpty());
    CHECK(!QFile::exists("icons:open.svg"));
}

static void testResource() {
    QResource r(":/hello.txt");
    CHECK(r.isValid());
    CHECK(r.fileName() == ":/hello.txt");
    CHECK(r.absoluteFilePath() == ":/hello.txt");
    CHECK(r.size() == 18);
    CHECK(r.uncompressedSize() == 18);
    CHECK(r.data() && QByteArray(reinterpret_cast<const char *>(r.data()), 18) == "Hello, resources!\n");
    CHECK(r.uncompressedData() == "Hello, resources!\n");
    CHECK(r.compressionAlgorithm() == QResource::NoCompression);
    CHECK(r.lastModified().isValid());

    r.setFileName("/data/../renamed.txt");
    CHECK(r.absoluteFilePath() == ":/renamed.txt");
    CHECK(r.uncompressedData() == "first\nsecond\nthird\n");

    r.setFileName(":/missing");
    CHECK(!r.isValid());
    CHECK(r.data() == nullptr);
    CHECK(r.size() == 0);
    CHECK(r.uncompressedData().isEmpty());
    CHECK(!r.lastModified().isValid());

    ResourceProbe icons(":/icons");
    CHECK(icons.isValid() && icons.isDir() && !icons.isFile());
    CHECK(icons.children() == (QStringList() << "open.svg" << "sub"));
    CHECK(icons.data() == nullptr);
    ResourceProbe file(":/icons/open.svg");
    CHECK(file.isFile() && !file.isDir() && file.children().isEmpty());

    // Variants for a locale: language and territory, then language, then every locale.
    QResource greeting(":/text/greeting.txt", QLocale::c());
    CHECK(greeting.uncompressedData() == "Hello\n");
    greeting.setLocale(QLocale(QString::fromUtf8("fr_FR")));
    CHECK(greeting.locale().name() == "fr_FR");
    CHECK(greeting.uncompressedData() == "Bonjour\n");
    greeting.setLocale(QLocale(QString::fromUtf8("fr_CA")));
    CHECK(greeting.uncompressedData() == "Bonjour\n");
    greeting.setLocale(QLocale(QString::fromUtf8("de_DE")));
    CHECK(greeting.uncompressedData() == "Hello\n");
    ResourceProbe text(":/text");
    CHECK(text.children() == (QStringList() << "a & b.txt" << "greeting.txt"));
}

static void testCmakeForms() {
    // nt_add_resources(<target> <name> FILES ...) names files relative to BASE.
    CHECK(readAll(":/listed/hello.txt") == "Hello, resources!\n");
    CHECK(QFileInfo(":/listed/icons/open.svg").isFile());
    // AUTORCC compiled tests/resources/autorcc.qrc.
    CHECK(readAll(":/auto/hello.txt") == "Hello, resources!\n");
}

static void testInitMacros() {
    // The resources registered themselves at start-up; the macros count registrations.
    Q_INIT_RESOURCE(test);
    CHECK(QFile::exists(":/hello.txt"));
    Q_CLEANUP_RESOURCE(test);
    CHECK(QFile::exists(":/hello.txt"));
}

static void testBinary() {
    const QString rcc = QString::fromUtf8(NT_RESOURCE_RCC);
    CHECK(!QFile::exists(":/bin/bin.txt"));
    CHECK(!QResource::registerResource(rcc, "bin")); // the root must start with a slash
    CHECK(QResource::registerResource(rcc, "/bin"));
    CHECK(readAll(":/bin/bin.txt") == "first\nsecond\nthird\n");
    CHECK(readAll(":/bin/deep/hello.txt") == "Hello, resources!\n");
    CHECK(QDir(":/").entryList().contains("bin"));
    CHECK(QDir(":/bin").entryList() == (QStringList() << "bin.txt" << "deep"));

    // An open file keeps the data of an unregistered tree alive.
    QFile open(":/bin/bin.txt");
    CHECK(open.open(QIODevice::ReadOnly));
    CHECK(QResource::unregisterResource(rcc, "/bin"));
    CHECK(!QFile::exists(":/bin/bin.txt"));
    CHECK(open.readAll() == "first\nsecond\nthird\n");
    CHECK(!QResource::unregisterResource(rcc, "/bin"));

    // Registered twice, it stays until it is unregistered twice.
    CHECK(QResource::registerResource(rcc, "/a/b"));
    CHECK(QResource::registerResource(rcc, "/a/b"));
    CHECK(QDir(":/a").entryList() == QStringList("b"));
    CHECK(QResource::unregisterResource(rcc, "/a/b"));
    CHECK(QFile::exists(":/a/b/bin.txt"));
    CHECK(QResource::unregisterResource(rcc, "/a/b"));
    CHECK(!QFile::exists(":/a/b/bin.txt"));

    // The same data registered from memory.
    QFile in(rcc);
    CHECK(in.open(QIODevice::ReadOnly));
    const QByteArray bytes = in.readAll();
    const auto *data = reinterpret_cast<const uchar *>(bytes.constData());
    CHECK(QResource::registerResource(data, "/mem"));
    CHECK(readAll(":/mem/bin.txt") == "first\nsecond\nthird\n");
    CHECK(QResource::unregisterResource(data, "/mem"));
    CHECK(!QFile::exists(":/mem/bin.txt"));
    CHECK(!QResource::registerResource(static_cast<const uchar *>(nullptr)));

    // Files that are not resource trees, or are damaged, are refused.
    const QString bad = g_tmp + "/bad.rcc";
    CHECK(writeFile(bad, QByteArray("not a resource file at all, really not")));
    CHECK(!QResource::registerResource(bad));
    CHECK(writeFile(bad, bytes.left(bytes.size() / 2)));
    CHECK(!QResource::registerResource(bad));
    QByteArray damaged = bytes;
    damaged[12] = '\x7f'; // a node count far beyond the file
    CHECK(writeFile(bad, damaged));
    CHECK(!QResource::registerResource(bad));
    damaged = bytes;
    damaged[36 + 20 + 3] = '\0'; // the root's first child is the root itself
    CHECK(writeFile(bad, damaged));
    CHECK(!QResource::registerResource(bad));
    CHECK(!QResource::registerResource(g_tmp + "/does-not-exist.rcc"));
    CHECK(!QFile::exists(":/bin.txt"));
}

static int runRcc(const QStringList &args, QByteArray *out = nullptr) {
    QProcess p;
    p.start(QString::fromUtf8(NT_RCC), args);
    if (!p.waitForFinished(30000))
        return -1;
    if (out)
        *out = p.readAllStandardOutput();
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

static void testCompiler() {
    const QString source = QString::fromUtf8(NT_RESOURCE_SOURCE_DIR);
    QByteArray out;
    CHECK(runRcc(QStringList() << "--version", &out) == 0);
    CHECK(out.contains("ntrcc"));
    CHECK(runRcc(QStringList() << "--list" << source + "/test.qrc", &out) == 0);
    CHECK(out.contains((source + "/hello.txt\n").toUtf8()));
    CHECK(out.contains((source + "/icons/sub/close.svg\n").toUtf8()));
    CHECK(runRcc(QStringList() << "-list-mapping" << source + "/binary.qrc", &out) == 0);
    CHECK(out.startsWith("/bin.txt\t"));

    // The C++ output names its functions after --name, made into an identifier.
    CHECK(runRcc(QStringList() << "--name" << "my-icons" << source + "/autorcc.qrc", &out) == 0);
    CHECK(out.contains("int qInitResources_my_icons()"));
    CHECK(out.contains("int qCleanupResources_my_icons()"));

    // A binary file written with --root mounts its files below the root.
    const QString rcc = g_tmp + "/rooted.rcc";
    CHECK(runRcc(QStringList() << "--binary" << "--root" << "/r" << "--no-compress" << "-o" << rcc
                               << source + "/autorcc.qrc") == 0);
    CHECK(QResource::registerResource(rcc));
    CHECK(readAll(":/r/auto/hello.txt") == "Hello, resources!\n");
    CHECK(QResource::unregisterResource(rcc));

    // Malformed input is an error.
    const QString qrc = g_tmp + "/bad.qrc";
    CHECK(writeFile(qrc, "<RCC><qresource><file>hello.txt</qresource></RCC>"));
    CHECK(runRcc(QStringList() << qrc) == 1);
    CHECK(writeFile(qrc, "<RCC><qresource><file>missing.txt</file></qresource></RCC>"));
    CHECK(runRcc(QStringList() << qrc) == 1);
    CHECK(writeFile(qrc, "<RCC><other/></RCC>"));
    CHECK(runRcc(QStringList() << qrc) == 1);
    CHECK(runRcc(QStringList() << "--unknown-option" << source + "/autorcc.qrc") == 1);
    CHECK(runRcc(QStringList()) == 1);
}

// Compressed resources: written by ntrcc into a binary file, read back through QResource and QFile.
static void testCompression() {
    QByteArray big;
    for (int i = 0; i < 200; ++i)
        big += "Line " + QByteArray::number(i % 7) + " of a text that compresses well.\n";
    CHECK(writeFile(g_tmp + "/big.txt", big));
    CHECK(writeFile(g_tmp + "/small.txt", "abc\n"));
    QByteArray qrc = "<RCC><qresource>\n"
                     "<file alias=\"best.txt\">big.txt</file>\n"
                     "<file alias=\"none.txt\" compression-algorithm=\"none\">big.txt</file>\n"
                     "<file alias=\"small.txt\" threshold=\"0\">small.txt</file>\n";
#ifdef NT_HAVE_ZLIB
    qrc += "<file alias=\"zlib.txt\" compression-algorithm=\"zlib\" compress=\"9\">big.txt</file>\n";
#endif
    qrc += "</qresource></RCC>\n";
    CHECK(writeFile(g_tmp + "/compressed.qrc", qrc));
    const QString rcc = g_tmp + "/compressed.rcc";
    CHECK(runRcc(QStringList() << "--binary" << "--root" << "/c" << "-o" << rcc << g_tmp + "/compressed.qrc") == 0);
    CHECK(QResource::registerResource(rcc));

#if defined(NT_HAVE_ZSTD)
    const QResource::Compression best = QResource::ZstdCompression;
#elif defined(NT_HAVE_ZLIB)
    const QResource::Compression best = QResource::ZlibCompression;
#else
    const QResource::Compression best = QResource::NoCompression;
#endif
    QResource r(":/c/best.txt");
    CHECK(r.compressionAlgorithm() == best);
    CHECK(r.uncompressedSize() == big.size());
    CHECK(best == QResource::NoCompression ? r.size() == big.size() : r.size() < big.size() / 4);
    CHECK(r.uncompressedData() == big);
    CHECK(readAll(":/c/best.txt") == big);
    CHECK(QFileInfo(":/c/best.txt").size() == big.size());
    CHECK(QFile(":/c/best.txt").size() == big.size());
    QFile f(":/c/best.txt");
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.size() == big.size());
    CHECK(f.readLine() == "Line 0 of a text that compresses well.\n");
    CHECK(f.seek(big.size() - 6));
    CHECK(f.readAll() == "well.\n");

    QResource none(":/c/none.txt");
    CHECK(none.compressionAlgorithm() == QResource::NoCompression);
    CHECK(none.size() == big.size() && none.uncompressedSize() == big.size());
    CHECK(readAll(":/c/none.txt") == big);
    // A file that compression does not make smaller is stored as is, whatever the threshold.
    CHECK(QResource(":/c/small.txt").compressionAlgorithm() == QResource::NoCompression);
    CHECK(readAll(":/c/small.txt") == "abc\n");

#ifdef NT_HAVE_ZLIB
    QResource zlib(":/c/zlib.txt");
    CHECK(zlib.compressionAlgorithm() == QResource::ZlibCompression);
    CHECK(zlib.uncompressedData() == big);
    CHECK(readAll(":/c/zlib.txt") == big);
#endif
    CHECK(QResource::unregisterResource(rcc));

    // The command line: --no-compress, and the threshold that keeps files as they are.
    CHECK(runRcc(QStringList() << "--binary" << "--no-compress" << "--root" << "/n" << "-o" << rcc
                               << g_tmp + "/compressed.qrc") == 0);
    CHECK(QResource::registerResource(rcc));
    CHECK(QResource(":/n/best.txt").compressionAlgorithm() == QResource::NoCompression);
    CHECK(QResource::unregisterResource(rcc));
    CHECK(runRcc(QStringList() << "--binary" << "--threshold" << "100" << "--root" << "/t" << "-o" << rcc
                               << g_tmp + "/compressed.qrc") == 0);
    CHECK(QResource::registerResource(rcc));
    CHECK(QResource(":/t/best.txt").compressionAlgorithm() == QResource::NoCompression);
    CHECK(QResource::unregisterResource(rcc));

    // Bad compression settings are errors.
    const QString in = g_tmp + "/compressed.qrc";
    CHECK(runRcc(QStringList() << "--compress-algo" << "lzma" << in) == 1);
    CHECK(runRcc(QStringList() << "--threshold" << "101" << in) == 1);
    CHECK(runRcc(QStringList() << "--compress" << "x" << in) == 1);
    CHECK(writeFile(g_tmp + "/badattr.qrc", "<RCC><qresource><file compress=\"99\">big.txt</file></qresource></RCC>"));
    CHECK(runRcc(QStringList() << g_tmp + "/badattr.qrc") == 1);
#ifdef NT_HAVE_ZLIB
    CHECK(runRcc(QStringList() << "--compress-algo" << "zlib" << "--compress" << "12" << in) == 1);
#else
    CHECK(runRcc(QStringList() << "--compress-algo" << "zlib" << in) == 1);
#endif
#ifndef NT_HAVE_ZSTD
    CHECK(runRcc(QStringList() << "--compress-algo" << "zstd" << in) == 1);
#endif

#ifdef NT_HAVE_ZLIB
    // Damaged compressed data cannot be read, but the rest of the tree can. zlib.txt sorts last, so
    // its data ends the file, and the last byte is part of its checksum.
    CHECK(runRcc(QStringList() << "--binary" << "--root" << "/d" << "-o" << rcc << in) == 0);
    QFile file(rcc);
    CHECK(file.open(QIODevice::ReadOnly));
    QByteArray bytes = file.readAll();
    file.close();
    bytes[bytes.size() - 1] = static_cast<char>(bytes.at(bytes.size() - 1) ^ 0x55);
    CHECK(writeFile(rcc, bytes));
    CHECK(QResource::registerResource(rcc));
    QResource damaged(":/d/zlib.txt");
    CHECK(damaged.isValid() && damaged.compressionAlgorithm() == QResource::ZlibCompression);
    CHECK(damaged.uncompressedData().isEmpty());
    QFile broken(":/d/zlib.txt");
    CHECK(!broken.open(QIODevice::ReadOnly));
    CHECK(broken.error() == QFileDevice::OpenError);
    CHECK(readAll(":/d/none.txt") == big);
    CHECK(QResource::unregisterResource(rcc));
#endif

    // Unknown flags, or a directory marked as compressed, are refused.
    CHECK(runRcc(QStringList() << "--binary" << "-o" << rcc << in) == 0);
    QFile again(rcc);
    CHECK(again.open(QIODevice::ReadOnly));
    const QByteArray good = again.readAll();
    QByteArray flagged = good;
    flagged[36 + 11] = '\x09'; // the root directory: directory and an unknown bit
    CHECK(writeFile(rcc, flagged));
    CHECK(!QResource::registerResource(rcc));
    flagged[36 + 11] = '\x03'; // the root directory, compressed with zlib
    CHECK(writeFile(rcc, flagged));
    CHECK(!QResource::registerResource(rcc));
}

int main() {
    const char *tmp = std::getenv("TMPDIR");
    const QString tmpBase = tmp && *tmp ? QDir::cleanPath(QString::fromUtf8(tmp)) : QString::fromUtf8("/tmp");
    g_tmp = tmpBase + "/nt_resource_test_" + QString::number(static_cast<qlonglong>(::getpid()));
    if (!QDir().mkpath(g_tmp)) {
        std::printf("cannot create %s\n", g_tmp.toUtf8().constData());
        return 1;
    }

    testFile();
    testFileInfo();
    testDir();
    testDirIterator();
    testSearchPaths();
    testResource();
    testCmakeForms();
    testInitMacros();
    testBinary();
    testCompiler();
    testCompression();

    CHECK(QDir(g_tmp).removeRecursively());
    if (g_failures == 0)
        std::puts("all resource tests passed");
    return g_failures == 0 ? 0 : 1;
}
