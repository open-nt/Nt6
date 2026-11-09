// MIT License - see LICENSE
#pragma once

// Resource lookup shared by QResource, QFile, QFileInfo and QDir. Paths that start with ':' name
// resources: ":/icons/open.png" is the file "icons/open.png" of the resource tree.

#include <QtCore/qlocale.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>

namespace NtResource {

// Format of a compiled resource tree (written by ntrcc). All numbers are big-endian.
//
//   header (36 bytes)
//     0  magic "NtRC"
//     4  u32 format version (1)
//     8  u32 total size in bytes
//    12  u32 node count (at least 1; node 0 is the root directory)
//    16  u32 offset of the node table
//    20  u32 offset of the string table
//    24  u32 size of the string table
//    28  u32 offset of the file data
//    32  u32 size of the file data
//
//   node (40 bytes)
//     0  u32 offset of the name in the string table (UTF-8, not terminated)
//     4  u32 length of the name
//     8  u32 flags: bit 0 set for a directory
//    12  u32 offset of the locale name in the string table ("", "fr" or "fr_FR")
//    16  u32 length of the locale name
//    20  u32 directory: index of the first child; file: offset of the data in the data section
//    24  u32 directory: number of children; file: size of the data
//    28  u32 reserved (0)
//    32  i64 last modification time in milliseconds since the epoch, or 0 if unknown
//
// The children of a directory are consecutive, sorted by name (bytewise) and then by locale, and
// always come after their parent, so the tree has no cycles. Files with the same name and different
// locales are siblings.
constexpr char kMagic[4] = {'N', 't', 'R', 'C'};
constexpr unsigned kVersion = 1;
constexpr unsigned kHeaderSize = 36;
constexpr unsigned kNodeSize = 40;
constexpr unsigned kDirectoryFlag = 1;

// What a resource path refers to.
struct Entry {
    bool exists = false;
    bool dir = false;
    const uchar *data = nullptr; // file contents, valid while keepAlive (or the registration) lives
    qint64 size = 0;
    qint64 lastModified = 0; // milliseconds since the epoch, 0 if unknown
    QStringList children;    // names of the entries of a directory, sorted
    std::shared_ptr<const void> keepAlive;
};

// Returns true if path names a resource, which is when it starts with ':'.
inline bool isResourcePath(const QString &path) { return path.startsWith(QChar(u':')); }

// Looks up the resource path (":/a/b", ":a/b" or "/a/b"), choosing the file variant that best fits
// locale. The children of a directory are listed only if withChildren is set.
Entry find(const QString &path, const QLocale &locale = QLocale(), bool withChildren = false);

} // namespace NtResource
