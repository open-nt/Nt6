// MIT License - see LICENSE
#include <QtCore/qstorageinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

#include <algorithm>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

/*!
    \class QStorageInfo
    \brief The QStorageInfo class provides information about currently mounted storage and drives.
    \since 5.4

    A QStorageInfo describes the volume that holds a file or directory: where it is mounted, the
    device and file system behind it, and how much space it has. Create one for a path, or list
    every volume with mountedVolumes():

    \code
    const QStorageInfo storage(QDir::homePath());
    if (storage.isValid() && storage.isReady() && storage.bytesAvailable() < needed)
        qWarning() << "not enough space on" << storage.displayName();

    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes())
        qDebug() << volume.rootPath() << volume.fileSystemType() << volume.bytesTotal();
    \endcode

    The object is a snapshot: the sizes are those of the moment it was created or last refreshed
    with refresh().

    \differences
    Mounted volumes are read from \c /proc/self/mountinfo, so the class describes volumes on Linux
    and Android; elsewhere every path is reported as being on the root volume, whose device and file
    system type are empty. mountedVolumes() leaves out the kernel's own file systems, such as
    \c proc, \c sysfs and the control groups, and of several volumes mounted on one directory only
    the one on top. name() is the label that udev lists in \c /dev/disk/by-label. subvolume() is the
    path of the mounted subvolume on Btrfs and empty for other file systems.
    \enddifferences

    \sa QDir, QFileInfo
*/

namespace {

struct Mount {
    QString mountPoint;
    QByteArray root;
    QByteArray fileSystemType;
    QByteArray device;
};

#ifdef __linux__

// The kernel writes a space, tab, newline or backslash in a path as a backslash and three octal digits.
QByteArray unescaped(const std::string &field) {
    QByteArray out;
    for (std::size_t i = 0; i < field.size(); ++i) {
        const auto octal = [&](std::size_t at) { return at < field.size() && field[at] >= '0' && field[at] <= '7'; };
        if (field[i] == '\\' && octal(i + 1) && octal(i + 2) && octal(i + 3)) {
            out.append(static_cast<char>((field[i + 1] - '0') * 64 + (field[i + 2] - '0') * 8 + (field[i + 3] - '0')));
            i += 3;
        } else {
            out.append(field[i]);
        }
    }
    return out;
}

// A line of mountinfo: id, parent, device number, root, mount point, options, optional fields that end with "-",
// file system type, source and the options of the super block.
bool parseMount(const std::string &line, Mount *mount) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t end = std::min(line.find(' ', start), line.size());
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    const auto separator =
        std::find(fields.begin() + std::min<std::ptrdiff_t>(6, static_cast<std::ptrdiff_t>(fields.size())),
                  fields.end(), std::string("-"));
    if (fields.size() < 7 || fields.end() - separator < 3)
        return false;
    mount->root = unescaped(fields[3]);
    mount->mountPoint = QFile::decodeName(unescaped(fields[4]));
    mount->fileSystemType = unescaped(separator[1]);
    mount->device = unescaped(separator[2]);
    return true;
}

std::vector<Mount> readMounts() {
    std::vector<Mount> mounts;
    QFile file(QString("/proc/self/mountinfo"));
    if (!file.open(QIODevice::ReadOnly))
        return mounts;
    const QByteArray all = file.readAll();
    qsizetype start = 0;
    while (start < all.size()) {
        qsizetype end = all.indexOf('\n', start);
        if (end < 0)
            end = all.size();
        Mount mount;
        if (parseMount(std::string(all.constData() + start, static_cast<std::size_t>(end - start)), &mount))
            mounts.push_back(mount);
        start = end + 1;
    }
    return mounts;
}

// The file systems that the kernel makes up; they hold no files of the user.
bool isPseudo(const Mount &mount) {
    static const char *const types[] = {"autofs",    "binfmt_misc", "bpf",      "cgroup",   "cgroup2",    "configfs",
                                        "debugfs",   "devpts",      "devtmpfs", "efivarfs", "fusectl",    "hugetlbfs",
                                        "mqueue",    "nsfs",        "proc",     "pstore",   "rpc_pipefs", "securityfs",
                                        "selinuxfs", "sysfs",       "tracefs"};
    return std::any_of(std::begin(types), std::end(types),
                       [&](const char *type) { return mount.fileSystemType == QByteArray(type); });
}

// udev writes a character of a label that is not safe in a file name as \x and two hexadecimal digits.
QString decodedLabel(const QString &fileName) {
    const QByteArray raw = fileName.toUtf8();
    QByteArray out;
    for (qsizetype i = 0; i < raw.size(); ++i) {
        bool ok = false;
        const int code =
            i + 3 < raw.size() && raw.at(i) == '\\' && raw.at(i + 1) == 'x' ? raw.mid(i + 2, 2).toInt(&ok, 16) : 0;
        if (ok) {
            out.append(static_cast<char>(code));
            i += 3;
        } else {
            out.append(raw.at(i));
        }
    }
    return QString::fromUtf8(out);
}

QString labelOf(const QByteArray &device) {
    if (!device.startsWith('/'))
        return QString();
    const QString wanted = QFileInfo(QFile::decodeName(device)).canonicalFilePath();
    if (wanted.isEmpty())
        return QString();
    const QDir labels(QString("/dev/disk/by-label"));
    const QFileInfoList entries =
        labels.entryInfoList(QDir::Filters(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot));
    for (const QFileInfo &entry : entries) {
        if (entry.canonicalFilePath() == wanted)
            return decodedLabel(entry.fileName());
    }
    return QString();
}

#else

std::vector<Mount> readMounts() {
    Mount root;
    root.mountPoint = QString("/");
    root.root = QByteArray("/");
    return {root};
}
bool isPseudo(const Mount &) { return false; }
QString labelOf(const QByteArray &) { return QString(); }

#endif

bool isUnder(const QString &path, const QString &mountPoint) {
    if (mountPoint == QString("/") || path == mountPoint)
        return true;
    return path.startsWith(mountPoint) && path.at(mountPoint.size()) == QChar(u'/');
}

} // namespace

/*!
    Constructs an invalid storage object: isValid() returns \c false, the strings are empty and
    the sizes are -1.

    \sa setPath()
*/
QStorageInfo::QStorageInfo() = default;

/*!
    Constructs a storage object for the volume that holds \a path, which may be a file or a
    directory anywhere on the volume. The object is invalid if \a path does not exist.

    \sa setPath()
*/
QStorageInfo::QStorageInfo(const QString &path) { setPath(path); }

/*!
    \overload
    Constructs a storage object for the volume that holds the directory \a dir.
*/
QStorageInfo::QStorageInfo(const QDir &dir) { setPath(dir.absolutePath()); }

/*!
    Makes this object describe the volume that holds \a path, and reads its sizes. The object
    becomes invalid if \a path does not exist.

    \sa rootPath(), refresh()
*/
void QStorageInfo::setPath(const QString &path) {
    d = Data();
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty() || !canonical.startsWith(QChar(u'/')))
        return;
    // The volume is the one mounted deepest on the way to the path; of several on one directory, the last.
    const std::vector<Mount> mounts = readMounts();
    const Mount *best = nullptr;
    for (const Mount &mount : mounts) {
        if (isUnder(canonical, mount.mountPoint) && (!best || mount.mountPoint.size() >= best->mountPoint.size()))
            best = &mount;
    }
    if (!best)
        return;
    d.rootPath = best->mountPoint;
    d.device = best->device;
    d.fileSystemType = best->fileSystemType;
    if (best->fileSystemType == QByteArray("btrfs") && best->root != QByteArray("/"))
        d.subvolume = best->root.startsWith('/') ? best->root.mid(1) : best->root;
    d.valid = true;
    refresh();
}

/*!
    Returns the directory on which the volume is mounted, such as \c{/} or \c{/home}. It is empty
    for an invalid object.

    \sa isRoot(), device()
*/
QString QStorageInfo::rootPath() const { return d.rootPath; }

/*!
    Returns the device of the volume as the system names it, such as \c{/dev/nvme0n1p2}. For a
    file system without a device, this is what was mounted, such as \c tmpfs or the address of a
    network share.

    \sa rootPath(), subvolume()
*/
QByteArray QStorageInfo::device() const { return d.device; }

/*!
    \since 5.9

    Returns the subvolume that is mounted, for a file system that has them, without a leading
    slash. It is empty when the whole file system is mounted.

    \sa device()
*/
QByteArray QStorageInfo::subvolume() const { return d.subvolume; }

/*!
    Returns the type of the file system, such as \c ext4, \c btrfs or \c tmpfs.

    \sa name()
*/
QByteArray QStorageInfo::fileSystemType() const { return d.fileSystemType; }

/*!
    Returns the name that the user gave the volume, its label, or an empty string if it has none
    or the label cannot be found.

    \sa displayName()
*/
QString QStorageInfo::name() const { return d.name; }

/*!
    Returns the name() of the volume, or rootPath() if the volume has no name. Use it to present
    the volume to the user.
*/
QString QStorageInfo::displayName() const { return d.name.isEmpty() ? d.rootPath : d.name; }

/*!
    Returns the size of the volume in bytes, or -1 if the object is invalid or not ready.

    \sa bytesFree(), bytesAvailable()
*/
qint64 QStorageInfo::bytesTotal() const { return d.bytesTotal; }

/*!
    Returns the number of free bytes on the volume, or -1 if the object is invalid or not ready.
    This counts the space that is reserved for the administrator; a program without privileges
    can write only bytesAvailable() bytes.

    \sa bytesTotal(), bytesAvailable()
*/
qint64 QStorageInfo::bytesFree() const { return d.bytesFree; }

/*!
    Returns the number of bytes that the current user can write to the volume, or -1 if the object
    is invalid or not ready.

    \sa bytesTotal(), bytesFree()
*/
qint64 QStorageInfo::bytesAvailable() const { return d.bytesAvailable; }

/*!
    \since 5.6

    Returns the block size of the file system that is best for reading and writing, in bytes, or
    -1 if it is not known.
*/
int QStorageInfo::blockSize() const { return d.blockSize; }

/*!
    Returns \c true if this is the volume that holds the root directory of the system.

    \sa root()
*/
bool QStorageInfo::isRoot() const { return d.valid && *this == root(); }

/*!
    Returns \c true if the volume is mounted read-only.
*/
bool QStorageInfo::isReadOnly() const { return d.readOnly; }

/*!
    Returns \c true if the volume can be used and its sizes could be read. A volume is not ready,
    for example, when the medium is not in the drive.

    \sa isValid()
*/
bool QStorageInfo::isReady() const { return d.ready; }

/*!
    Returns \c true if the object describes a mounted volume, that is, if the path it was given
    exists.

    \sa isReady()
*/
bool QStorageInfo::isValid() const { return d.valid; }

/*!
    Reads the sizes, the read-only state and the name of the volume again. The object describes
    the same root path as before.

    \sa bytesAvailable()
*/
void QStorageInfo::refresh() {
    if (!d.valid)
        return;
    d.bytesTotal = d.bytesFree = d.bytesAvailable = -1;
    d.blockSize = -1;
    d.ready = false;
#ifndef _WIN32
    struct statvfs info = {};
    if (statvfs(QFile::encodeName(d.rootPath).constData(), &info) == 0) {
        const auto bytes = [&](fsblkcnt_t blocks) {
            return static_cast<qint64>(blocks) * static_cast<qint64>(info.f_frsize);
        };
        d.bytesTotal = bytes(info.f_blocks);
        d.bytesFree = bytes(info.f_bfree);
        d.bytesAvailable = bytes(info.f_bavail);
        d.blockSize = static_cast<int>(info.f_bsize);
        d.readOnly = (info.f_flag & ST_RDONLY) != 0;
        d.ready = true;
    }
#endif
    d.name = labelOf(d.device);
}

/*!
    Returns the volumes that are mounted now, in the order the system lists them. The root volume
    is among them.

    \sa root()
*/
QList<QStorageInfo> QStorageInfo::mountedVolumes() {
    QList<QStorageInfo> volumes;
    const std::vector<Mount> mounts = readMounts();
    for (std::size_t i = 0; i < mounts.size(); ++i) {
        const Mount &mount = mounts[i];
        // A later mount on the same directory hides this one.
        const bool hidden = std::any_of(mounts.begin() + static_cast<std::ptrdiff_t>(i) + 1, mounts.end(),
                                        [&](const Mount &later) { return later.mountPoint == mount.mountPoint; });
        if (hidden || isPseudo(mount))
            continue;
        QStorageInfo volume;
        volume.d.rootPath = mount.mountPoint;
        volume.d.device = mount.device;
        volume.d.fileSystemType = mount.fileSystemType;
        if (mount.fileSystemType == QByteArray("btrfs") && mount.root != QByteArray("/"))
            volume.d.subvolume = mount.root.startsWith('/') ? mount.root.mid(1) : mount.root;
        volume.d.valid = true;
        volume.refresh();
        volumes.append(volume);
    }
    return volumes;
}

/*!
    Returns the volume that holds the root directory of the system.

    \sa isRoot()
*/
QStorageInfo QStorageInfo::root() { return QStorageInfo(QString("/")); }
