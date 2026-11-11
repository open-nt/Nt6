// MIT License - see LICENSE

#include <QtCore/qresource.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

#include "qresource_p.h"

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif
#ifdef NT_HAVE_ZSTD
#include <zstd.h>
#endif

#include <algorithm>
#include <cstring>
#include <new>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

quint32 read32(const uchar *p) {
    return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) |
           (static_cast<quint32>(p[2]) << 8) | static_cast<quint32>(p[3]);
}

qint64 read64(const uchar *p) {
    const quint64 v = (static_cast<quint64>(read32(p)) << 32) | read32(p + 4);
    return static_cast<qint64>(v);
}

// Splits a resource path into its components, resolving "." and ".." lexically. The leading ':' is
// optional, and the path is always taken from the root of the resources.
std::vector<std::string> components(const QString &path) {
    const QString p = NtResource::isResourcePath(path) ? path.mid(1) : path;
    std::vector<std::string> out;
    for (const QString &c : p.split(QChar(u'/'), Qt::SkipEmptyParts)) {
        if (c == QString::fromUtf8("."))
            continue;
        if (c == QString::fromUtf8("..")) {
            if (!out.empty())
                out.pop_back();
            continue;
        }
        out.push_back(c.toUtf8().toStdString());
    }
    return out;
}

QString joinPath(const std::vector<std::string> &parts) {
    std::string s;
    for (const std::string &p : parts)
        s += '/' + p;
    return QString::fromUtf8(s.empty() ? "/" : s.c_str());
}

// How well the locale of a file variant fits: 0 not at all, 1 a variant for every locale, 2 the
// language, 3 the language and territory.
int localeScore(std::string_view candidate, const std::string &name, const std::string &language) {
    if (candidate.empty())
        return 1;
    if (candidate == name)
        return 3;
    if (candidate == language)
        return 2;
    return 0;
}

struct Node {
    std::string_view name;
    std::string_view locale;
    bool dir = false;
    NtResource::Compression compression = NtResource::Compression::None;
    quint32 first = 0;
    quint32 count = 0;
    quint32 uncompressedSize = 0;
    qint64 lastModified = 0;
};

enum class Kind { None, File, Dir };

struct Hit {
    Kind kind = Kind::None;
    quint32 node = 0;
    std::string virtualChild; // set for a directory above the resource root
};

// One registered resource tree.
struct Tree {
    const uchar *base = nullptr;
    std::shared_ptr<const std::vector<uchar>> owned; // the contents of a registered .rcc file
    QString fileName;                                // absolute path of that file, empty for compiled-in data
    std::vector<std::string> root;                   // where the tree is mounted
    int refs = 1;
    quint32 nodeCount = 0;
    quint32 nodeTable = 0;
    quint32 strings = 0;
    quint32 dataOffset = 0;

    Node node(quint32 i) const {
        const uchar *p = base + nodeTable + static_cast<std::size_t>(i) * NtResource::kNodeSize;
        const char *s = reinterpret_cast<const char *>(base + strings);
        Node n;
        n.name = std::string_view(s + read32(p), read32(p + 4));
        const quint32 flags = read32(p + 8);
        n.dir = (flags & NtResource::kDirectoryFlag) != 0;
        if (flags & NtResource::kZlibFlag)
            n.compression = NtResource::Compression::Zlib;
        else if (flags & NtResource::kZstdFlag)
            n.compression = NtResource::Compression::Zstd;
        n.locale = std::string_view(s + read32(p + 12), read32(p + 16));
        n.first = read32(p + 20);
        n.count = read32(p + 24);
        n.uncompressedSize = n.compression == NtResource::Compression::None ? n.count : read32(p + 28);
        n.lastModified = read64(p + 32);
        return n;
    }

    // Reads and checks the header and every node, so that lookups can trust the offsets. available
    // is the number of readable bytes at base.
    bool load(quint64 available) {
        using namespace NtResource;
        if (available < kHeaderSize || std::memcmp(base, kMagic, sizeof kMagic) != 0 || read32(base + 4) != kVersion)
            return false;
        const quint64 total = read32(base + 8);
        const quint64 count = read32(base + 12), table = read32(base + 16);
        const quint64 strOffset = read32(base + 20), strSize = read32(base + 24);
        const quint64 dataOff = read32(base + 28), dataSize = read32(base + 32);
        if (total < kHeaderSize || total > available || count == 0 || table + count * kNodeSize > total ||
            strOffset + strSize > total || dataOff + dataSize > total)
            return false;
        nodeCount = static_cast<quint32>(count);
        nodeTable = static_cast<quint32>(table);
        strings = static_cast<quint32>(strOffset);
        dataOffset = static_cast<quint32>(dataOff);
        for (quint32 i = 0; i < nodeCount; ++i) {
            const uchar *p = base + nodeTable + static_cast<std::size_t>(i) * kNodeSize;
            if (quint64(read32(p)) + read32(p + 4) > strSize || quint64(read32(p + 12)) + read32(p + 16) > strSize)
                return false;
            const quint32 flags = read32(p + 8);
            const bool dir = (flags & kDirectoryFlag) != 0;
            const quint64 first = read32(p + 20), n = read32(p + 24);
            // A file has at most one compression algorithm, and a directory none.
            const quint32 compression = flags & (kZlibFlag | kZstdFlag);
            if ((flags & ~kKnownFlags) != 0 || compression == (kZlibFlag | kZstdFlag) || (dir && compression != 0))
                return false;
            if (i == 0 && !dir)
                return false;
            if (dir ? (n != 0 && (first <= i || first + n > nodeCount)) : first + n > dataSize)
                return false;
        }
        return true;
    }

    Hit resolve(const std::vector<std::string> &path, const std::string &locale, const std::string &language) const {
        Hit hit;
        const std::size_t common = std::min(path.size(), root.size());
        for (std::size_t i = 0; i < common; ++i)
            if (path[i] != root[i])
                return hit;
        if (path.size() < root.size()) {
            hit.kind = Kind::Dir;
            hit.virtualChild = root[path.size()];
            return hit;
        }
        quint32 current = 0;
        for (std::size_t i = root.size(); i < path.size(); ++i) {
            const Node dir = node(current);
            if (!dir.dir)
                return hit;
            const std::string_view name(path[i]);
            quint32 lo = dir.first, hi = dir.first + dir.count;
            while (lo < hi) {
                const quint32 mid = lo + (hi - lo) / 2;
                if (node(mid).name < name)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            const bool last = i + 1 == path.size();
            int best = 0;
            for (quint32 j = lo; j < dir.first + dir.count; ++j) {
                const Node child = node(j);
                if (child.name != name)
                    break;
                // A directory wins over files of the same name; it is found on the way to its entries.
                const int score = child.dir ? 4 : (last ? localeScore(child.locale, locale, language) : 0);
                if (score > best) {
                    best = score;
                    current = j;
                }
            }
            if (best == 0)
                return hit;
        }
        hit.kind = node(current).dir ? Kind::Dir : Kind::File;
        hit.node = current;
        return hit;
    }
};

struct Registry {
    std::mutex mutex;
    std::vector<std::shared_ptr<Tree>> trees;
};

// The registry is never destroyed, so resources compiled into other object files can unregister
// themselves from static destructors in any order.
Registry &registry() {
    static Registry *r = new Registry; // never freed, on purpose
    return *r;
}

bool rootComponents(const QString &resourceRoot, std::vector<std::string> *out) {
    if (!resourceRoot.isEmpty() && !resourceRoot.startsWith(QChar(u'/')))
        return false;
    *out = components(resourceRoot);
    return true;
}

bool addTree(std::shared_ptr<Tree> tree) {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    for (const std::shared_ptr<Tree> &t : r.trees) {
        const bool same = tree->owned ? t->fileName == tree->fileName && !t->fileName.isEmpty() : t->base == tree->base;
        if (same && t->root == tree->root) {
            ++t->refs;
            return true;
        }
    }
    r.trees.push_back(std::move(tree));
    return true;
}

template <typename Match> bool removeTree(const std::vector<std::string> &root, Match match) {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    for (auto it = r.trees.begin(); it != r.trees.end(); ++it) {
        if ((*it)->root != root || !match(**it))
            continue;
        if (--(*it)->refs == 0)
            r.trees.erase(it);
        return true;
    }
    return false;
}

} // namespace

NtResource::Entry NtResource::find(const QString &path, const QLocale &locale, bool withChildren) {
    Entry entry;
    const std::vector<std::string> parts = components(path);
    const std::string name = locale.name().toUtf8().toStdString();
    const std::string language = name.substr(0, name.find('_'));
    std::set<std::string> children;

    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    // The trees registered last are searched first, so they can override earlier ones.
    for (auto it = r.trees.rbegin(); it != r.trees.rend(); ++it) {
        const Tree &tree = **it;
        const Hit hit = tree.resolve(parts, name, language);
        if (hit.kind == Kind::None || (entry.exists && (!entry.dir || hit.kind != Kind::Dir)))
            continue;
        if (!entry.exists) {
            entry.exists = true;
            entry.dir = hit.kind == Kind::Dir;
            if (!entry.dir) {
                const Node file = tree.node(hit.node);
                entry.data = tree.base + tree.dataOffset + file.first;
                entry.size = file.count;
                entry.compression = file.compression;
                entry.uncompressedSize = file.uncompressedSize;
                entry.lastModified = file.lastModified;
                entry.keepAlive = tree.owned;
                break;
            }
        }
        if (!withChildren)
            continue;
        if (!hit.virtualChild.empty()) {
            children.insert(hit.virtualChild);
            continue;
        }
        const Node dir = tree.node(hit.node);
        for (quint32 j = dir.first; j < dir.first + dir.count; ++j)
            children.insert(std::string(tree.node(j).name));
    }
    for (const std::string &c : children)
        entry.children.append(QString::fromUtf8(c.c_str()));
    return entry;
}

bool NtResource::canDecompress(Compression c) {
#ifdef NT_HAVE_ZLIB
    constexpr bool zlib = true;
#else
    constexpr bool zlib = false;
#endif
#ifdef NT_HAVE_ZSTD
    constexpr bool zstd = true;
#else
    constexpr bool zstd = false;
#endif
    return c == Compression::None || (c == Compression::Zlib && zlib) || (c == Compression::Zstd && zstd);
}

bool NtResource::decompress(Entry &entry) {
    if (!entry.exists || entry.dir || entry.compression == Compression::None)
        return true;
    if (!canDecompress(entry.compression))
        return false;
    const auto expected = static_cast<std::size_t>(entry.uncompressedSize);
    std::shared_ptr<std::vector<uchar>> out;
    try {
        out = std::make_shared<std::vector<uchar>>(std::max<std::size_t>(expected, 1));
    } catch (const std::bad_alloc &) {
        return false;
    }
    bool ok = false;
    if (entry.compression == Compression::Zlib) {
#ifdef NT_HAVE_ZLIB
        uLongf length = static_cast<uLongf>(expected);
        ok = ::uncompress(out->data(), &length, entry.data, static_cast<uLong>(entry.size)) == Z_OK &&
             length == expected;
#endif
    } else {
#ifdef NT_HAVE_ZSTD
        // The size recorded in the frame must agree with the tree before anything is decompressed.
        ok = ZSTD_getFrameContentSize(entry.data, static_cast<std::size_t>(entry.size)) == expected;
        if (ok) {
            const std::size_t n =
                ZSTD_decompress(out->data(), expected, entry.data, static_cast<std::size_t>(entry.size));
            ok = !ZSTD_isError(n) && n == expected;
        }
#endif
    }
    if (!ok)
        return false;
    out->resize(expected);
    entry.data = out->data();
    entry.size = entry.uncompressedSize;
    entry.compression = Compression::None;
    entry.keepAlive = std::move(out);
    return true;
}

struct QResource::Private {
    QString file;
    QLocale locale;
    mutable bool resolved = false;
    mutable NtResource::Entry entry;

    const NtResource::Entry &get() const {
        if (!resolved) {
            entry = NtResource::find(file, locale, true);
            resolved = true;
        }
        return entry;
    }
};

/*!
    \class QResource
    \brief The QResource class provides an interface for reading directly from resources.

    Resources are files compiled into the program, or into a binary \c .rcc
    file, by \l{The Nt Resource System}{ntrcc}. They are read through paths
    that start with a colon, such as \c{:/icons/open.png}, by QFile, QFileInfo,
    QDir and QDirIterator, so most code never uses QResource directly.

    QResource gives access to the bytes of a resource without opening it, and
    registers and unregisters resource trees at run time:

    \code
    QResource::registerResource("/usr/share/app/themes.rcc", "/themes");
    QResource readme(":/themes/dark/README");
    if (readme.isValid())
        process(readme.data(), readme.size());
    \endcode

    A resource can exist in several variants for different locales. QResource
    picks the variant of locale(): one for its language and territory, then
    one for its language, then one for every locale.

    \differences
    Compressed resources need zlib or libzstd when Nt is built. The compiled
    format is Nt's own and is not compatible with the output of Qt's
    \c rcc. As in Qt 6, QResource has no search paths of its own; use
    QDir::setSearchPaths(). A resource file is read into memory rather than
    mapped. The data of a resource stays valid
    while the QResource exists or the tree is registered. The
    \c MapPrivateOption of registerResource() does not exist.

    A cross build, such as one for Zephyr, runs an \c ntrcc that stores
    resources uncompressed, as the target has neither zlib nor Zstandard. See
    \l{Nt on Zephyr}.
    \enddifferences

    \sa {The Nt Resource System}, QFile, QDir
*/

/*!
    Constructs a resource for the path \a file, choosing the variant for
    \a locale. The path starts with \c{:/} or \c{/}; both name the same
    resource.

    \sa setFileName(), setLocale()
*/
QResource::QResource(const QString &file, const QLocale &locale) : d(std::make_unique<Private>()) {
    d->file = file;
    d->locale = locale;
}

/*!
    Destroys the resource.
*/
QResource::~QResource() = default;

/*!
    Sets the path of the resource to \a file.

    \sa fileName(), absoluteFilePath()
*/
void QResource::setFileName(const QString &file) {
    d->file = file;
    d->resolved = false;
}

/*!
    Returns the path of the resource as it was set.

    \sa setFileName(), absoluteFilePath()
*/
QString QResource::fileName() const { return d->file; }

/*!
    Returns the cleaned path of the resource, which starts with \c{:/}.

    \sa fileName()
*/
QString QResource::absoluteFilePath() const { return QChar(u':') + joinPath(components(d->file)); }

/*!
    Sets the locale used to choose a variant of the resource to \a locale.

    \sa locale()
*/
void QResource::setLocale(const QLocale &locale) {
    d->locale = locale;
    d->resolved = false;
}

/*!
    Returns the locale used to choose a variant of the resource.

    \sa setLocale()
*/
QLocale QResource::locale() const { return d->locale; }

/*!
    Returns \c true if the resource exists, as a file or a directory;
    otherwise returns \c false.

    \sa isFile(), isDir()
*/
bool QResource::isValid() const { return d->get().exists; }

/*!
    \since 5.13
    Returns the compression algorithm of the data returned by data(), or
    \c NoCompression if it is stored as is. QFile and uncompressedData()
    decompress the data; they fail if Nt was built without the algorithm.

    \sa data(), uncompressedData()
*/
QResource::Compression QResource::compressionAlgorithm() const {
    return static_cast<Compression>(d->get().compression);
}

/*!
    Returns the size of the data in bytes, compressed if the resource is
    compressed, or \c 0 if the resource is not a file.

    \sa data(), uncompressedSize()
*/
qint64 QResource::size() const { return d->get().size; }

/*!
    Returns the data of the resource, or \c nullptr if it is not a file. The
    data is not terminated by a null byte, and is compressed if
    compressionAlgorithm() is not \c NoCompression; use uncompressedData()
    to read it either way.

    \sa size(), uncompressedData()
*/
const uchar *QResource::data() const { return d->get().data; }

/*!
    \since 5.15
    Returns the size of the uncompressed data, which equals size() if the
    resource is not compressed.

    \sa uncompressedData()
*/
qint64 QResource::uncompressedSize() const { return d->get().uncompressedSize; }

/*!
    \since 5.15
    Returns a copy of the data of the resource, decompressed if it is
    compressed. Returns an empty array if it is not a file, or if it cannot be
    decompressed: Nt was built without the algorithm, or the data is damaged.

    \sa data()
*/
QByteArray QResource::uncompressedData() const {
    NtResource::Entry e = d->get();
    if (!e.data || !NtResource::decompress(e))
        return QByteArray();
    return QByteArray(reinterpret_cast<const char *>(e.data), static_cast<qsizetype>(e.size));
}

/*!
    \since 5.8
    Returns the time the source file of the resource was last modified when
    it was compiled, or an invalid QDateTime if it is unknown.

    \sa QFileInfo::lastModified()
*/
QDateTime QResource::lastModified() const {
    const qint64 ms = d->get().lastModified;
    return ms == 0 ? QDateTime() : QDateTime::fromMSecsSinceEpoch(ms);
}

/*!
    Returns \c true if the resource is a directory; otherwise returns
    \c false.

    \sa isFile(), children()
*/
bool QResource::isDir() const { return d->get().dir; }

/*!
    Returns \c true if the resource is a file; otherwise returns \c false.

    \sa isDir()
*/
bool QResource::isFile() const { return d->get().exists && !d->get().dir; }

/*!
    Returns the sorted names of the entries of a directory resource, or an
    empty list if it is not a directory.

    \sa isDir()
*/
QStringList QResource::children() const { return d->get().children; }

/*!
    Registers the binary resource file \a rccFileName, written by
    \c{ntrcc --binary}, under \a resourceRoot, which is empty or starts with a
    slash. Returns \c true on success; otherwise returns \c false, for example
    if the file cannot be read or is not a valid resource file.

    The file is read into memory. Registering the same file and root again
    increments a count that unregisterResource() decrements.

    \sa unregisterResource()
*/
bool QResource::registerResource(const QString &rccFileName, const QString &resourceRoot) {
    auto tree = std::make_shared<Tree>();
    if (!rootComponents(resourceRoot, &tree->root))
        return false;
    QFile file(rccFileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.readAll();
    auto owned = std::make_shared<std::vector<uchar>>(bytes.begin(), bytes.end());
    tree->base = owned->data();
    tree->owned = std::move(owned);
    tree->fileName = NtResource::isResourcePath(rccFileName) ? QResource(rccFileName).absoluteFilePath()
                                                             : QFileInfo(rccFileName).absoluteFilePath();
    if (!tree->load(tree->owned->size()))
        return false;
    return addTree(std::move(tree));
}

/*!
    Unregisters the resource file \a rccFileName from \a resourceRoot.
    Returns \c true if it was registered; otherwise returns \c false.

    \sa registerResource()
*/
bool QResource::unregisterResource(const QString &rccFileName, const QString &resourceRoot) {
    std::vector<std::string> root;
    if (!rootComponents(resourceRoot, &root))
        return false;
    const QString name = NtResource::isResourcePath(rccFileName) ? QResource(rccFileName).absoluteFilePath()
                                                                 : QFileInfo(rccFileName).absoluteFilePath();
    return removeTree(root, [&](const Tree &t) { return t.owned && t.fileName == name; });
}

/*!
    \overload

    Registers the resource tree at \a rccData under \a resourceRoot, which is
    empty or starts with a slash. The data must stay valid until it is
    unregistered. Returns \c true on success; otherwise returns \c false.

    \sa unregisterResource()
*/
bool QResource::registerResource(const uchar *rccData, const QString &resourceRoot) {
    if (!rccData)
        return false;
    auto tree = std::make_shared<Tree>();
    if (!rootComponents(resourceRoot, &tree->root))
        return false;
    tree->base = rccData;
    if (!tree->load(std::numeric_limits<quint64>::max()))
        return false;
    return addTree(std::move(tree));
}

/*!
    \overload

    Unregisters the resource tree at \a rccData from \a resourceRoot.
    Returns \c true if it was registered; otherwise returns \c false.

    \sa registerResource()
*/
bool QResource::unregisterResource(const uchar *rccData, const QString &resourceRoot) {
    std::vector<std::string> root;
    if (!rccData || !rootComponents(resourceRoot, &root))
        return false;
    return removeTree(root, [&](const Tree &t) { return !t.owned && t.base == rccData; });
}
