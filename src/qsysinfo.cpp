// MIT License - see LICENSE
#include <sys/utsname.h>
#include <unistd.h>

#include <QtCore/qfile.h>
#include <QtCore/qoperatingsystemversion.h>
#include <QtCore/qsysinfo.h>

#include <cstring>
#include <map>
#include <string>

namespace {

// KEY=value lines of os-release(5); values may be quoted.
std::map<std::string, std::string> readOsRelease() {
    std::map<std::string, std::string> values;
    for (const char *path : {"/etc/os-release", "/usr/lib/os-release"}) {
        QFile file{QString::fromUtf8(path)};
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> lines = file.readAll().split('\n');
        for (const QByteArray &raw : lines) {
            const QByteArray line = raw.trimmed();
            const qsizetype eq = line.indexOf('=');
            if (line.isEmpty() || line.startsWith('#') || eq <= 0)
                continue;
            QByteArray value = line.mid(eq + 1);
            if (value.size() >= 2 && (value.startsWith('"') || value.startsWith('\'')) && value.endsWith(value.at(0)))
                value = value.mid(1, value.size() - 2);
            values[line.left(eq).toStdString()] = value.toStdString();
        }
        break;
    }
    return values;
}

QString osReleaseValue(const char *key) {
    const auto values = readOsRelease();
    const auto it = values.find(key);
    return it == values.end() ? QString() : QString::fromStdString(it->second);
}

QString unameField(const char *field) {
    utsname u;
    if (uname(&u) != 0)
        return QString();
    return QString::fromUtf8(std::strcmp(field, "machine") == 0   ? u.machine
                             : std::strcmp(field, "sysname") == 0 ? u.sysname
                                                                  : u.release);
}

QByteArray firstLineOf(const char *path) {
    QFile file{QString::fromUtf8(path)};
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readLine().trimmed();
}

} // namespace

/*!
    \class QSysInfo
    \brief The QSysInfo class provides information about the system.

    The class has only static functions. The \c build functions describe the
    system the program was compiled for, and the \c current functions the one
    it runs on, which may differ.

    \code
    qDebug() << QSysInfo::prettyProductName() << QSysInfo::currentCpuArchitecture();
    // "Ubuntu 24.04 LTS" "x86_64"
    \endcode

    On Linux the product information comes from \c os-release(5), the kernel
    information from \c uname(2), and the identifiers from \c /etc/machine-id
    and \c /proc/sys/kernel/random/boot_id.

    \differences
    There are no \c macVersion(), \c windowsVersion(), \c MacVersion or
    \c WinVersion members, which exist only on macOS and Windows.
    \enddifferences

    \sa QOperatingSystemVersion
*/

/*!
    Returns the architecture of the CPU that the program was built for, such as
    \c x86_64, \c i386, \c arm64, \c arm or \c riscv64.

    \sa currentCpuArchitecture(), buildAbi()
*/
QString QSysInfo::buildCpuArchitecture() {
#if defined(__x86_64__)
    return QStringLiteral("x86_64");
#elif defined(__i386__)
    return QStringLiteral("i386");
#elif defined(__aarch64__)
    return QStringLiteral("arm64");
#elif defined(__arm__)
    return QStringLiteral("arm");
#elif defined(__riscv) && __riscv_xlen == 64
    return QStringLiteral("riscv64");
#elif defined(__riscv)
    return QStringLiteral("riscv32");
#elif defined(__powerpc64__)
    return QStringLiteral("power64");
#elif defined(__s390x__)
    return QStringLiteral("s390x");
#else
    return QStringLiteral("unknown");
#endif
}

/*!
    Returns the architecture of the CPU that the program is running on, which
    can differ from buildCpuArchitecture() when a 32-bit program runs on a
    64-bit system or under emulation.

    \sa buildCpuArchitecture()
*/
QString QSysInfo::currentCpuArchitecture() {
    const QString machine = unameField("machine");
    if (machine == QStringLiteral("aarch64") || machine == QStringLiteral("arm64"))
        return QStringLiteral("arm64");
    if (machine.startsWith(QStringLiteral("arm")))
        return QStringLiteral("arm");
    if (machine == QStringLiteral("amd64"))
        return QStringLiteral("x86_64");
    if (machine.size() == 4 && machine.at(0) == QLatin1Char('i') && machine.endsWith(QStringLiteral("86")))
        return QStringLiteral("i386");
    return machine.isEmpty() ? QStringLiteral("unknown") : machine;
}

/*!
    Returns the ABI that the program was built for as a string of the form
    \c{architecture-endianness-wordsize}, for example
    \c{x86_64-little_endian-lp64}.

    \sa buildCpuArchitecture()
*/
QString QSysInfo::buildAbi() {
    const char *size = sizeof(void *) == 8 ? (sizeof(long) == 8 ? "lp64" : "llp64") : "ilp32";
    return buildCpuArchitecture() + QLatin1Char('-') +
           (ByteOrder == LittleEndian ? QStringLiteral("little_endian") : QStringLiteral("big_endian")) +
           QLatin1Char('-') + QString::fromLatin1(size);
}

/*!
    Returns the lower-case name of the kernel, such as \c linux.

    \sa kernelVersion()
*/
QString QSysInfo::kernelType() { return unameField("sysname").toLower(); }

/*!
    Returns the release of the kernel, for example \c{6.8.0-45-generic}.

    \sa kernelType()
*/
QString QSysInfo::kernelVersion() { return unameField("release"); }

/*!
    Returns the identifier of the operating system product, such as \c ubuntu
    or \c fedora, taken from the \c ID entry of \c os-release. Returns
    \c unknown if the system does not say.

    \sa productVersion(), prettyProductName()
*/
QString QSysInfo::productType() {
    const QString id = osReleaseValue("ID");
    return id.isEmpty() ? QStringLiteral("unknown") : id;
}

/*!
    Returns the version of the operating system product, such as \c{24.04},
    taken from the \c VERSION_ID entry of \c os-release. Returns \c unknown if
    the system does not say, as rolling releases do not.

    \sa productType()
*/
QString QSysInfo::productVersion() {
    const QString version = osReleaseValue("VERSION_ID");
    return version.isEmpty() ? QStringLiteral("unknown") : version;
}

/*!
    Returns a name for the operating system that is suitable for showing to a
    user, such as \c{Ubuntu 24.04 LTS}, taken from the \c PRETTY_NAME entry of
    \c os-release. Falls back to the kernel name and release.

    \sa productType(), productVersion()
*/
QString QSysInfo::prettyProductName() {
    QString pretty = osReleaseValue("PRETTY_NAME");
    if (!pretty.isEmpty())
        return pretty;
    return unameField("sysname") + QLatin1Char(' ') + unameField("release");
}

/*!
    Returns the host name of the machine, as set by the administrator, without
    the domain.
*/
QString QSysInfo::machineHostName() {
    char buffer[256] = {};
    if (gethostname(buffer, sizeof buffer - 1) != 0)
        return QString();
    QString name = QString::fromUtf8(buffer);
    const qsizetype dot = name.indexOf(QLatin1Char('.'));
    return dot > 0 ? name.left(dot) : name;
}

/*!
    Returns an identifier that is the same for the life of the installation of
    the operating system, from \c /etc/machine-id. Returns an empty array if
    the system has none. Do not use it as a secret.

    \sa bootUniqueId()
*/
QByteArray QSysInfo::machineUniqueId() {
    QByteArray id = firstLineOf("/etc/machine-id");
    return id.isEmpty() ? firstLineOf("/var/lib/dbus/machine-id") : id;
}

/*!
    Returns an identifier that is different after each boot of the machine,
    from \c /proc/sys/kernel/random/boot_id. Returns an empty array if the
    system has none.

    \sa machineUniqueId()
*/
QByteArray QSysInfo::bootUniqueId() { return firstLineOf("/proc/sys/kernel/random/boot_id"); }

/*!
    \class QOperatingSystemVersion
    \brief The QOperatingSystemVersion class provides information about the operating system version.

    An object holds an operating system family and a version of up to three
    numbers. current() returns the system the program runs on; other objects
    can be constructed to compare against:

    \code
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();
    if (os >= QOperatingSystemVersion(QOperatingSystemVersion::Unknown, 22))
        qDebug() << os.name() << os.version();
    \endcode

    The comparison operators only compare versions of the same operating
    system family; all of them are \c false for different families.

    On Linux the type is Unknown, the name is the \c NAME entry of
    \c os-release, and the version comes from its \c VERSION_ID.

    \differences
    There are no static constants for versions of Windows, macOS, iOS, tvOS,
    watchOS, visionOS and Android, such as \c Windows10 or \c AndroidOreo, and
    no function that tests a platform-specific API level. There is no
    \c OSType value for Linux, as in Qt.
    \enddifferences

    \sa QSysInfo, QVersionNumber
*/

/*!
    Returns the operating system the program is running on.

    \sa currentType()
*/
QOperatingSystemVersion QOperatingSystemVersion::current() {
    int parts[3] = {-1, -1, -1};
    const QStringList numbers = osReleaseValue("VERSION_ID").split(QLatin1Char('.'));
    for (qsizetype i = 0; i < 3 && i < numbers.size(); ++i) {
        bool ok = false;
        const int n = numbers.at(i).toInt(&ok);
        if (!ok)
            break;
        parts[i] = n;
    }
    QOperatingSystemVersion v(Unknown, parts[0], parts[1], parts[2]);
    v.m_name = osReleaseValue("NAME");
    if (v.m_name.isEmpty())
        v.m_name = QStringLiteral("Linux");
    return v;
}

/*!
    Returns the version as a QVersionNumber with as many segments as are set.
*/
QVersionNumber QOperatingSystemVersion::version() const {
    QList<int> segments;
    if (m_major >= 0)
        segments.append(m_major);
    if (m_major >= 0 && m_minor >= 0)
        segments.append(m_minor);
    if (m_major >= 0 && m_minor >= 0 && m_micro >= 0)
        segments.append(m_micro);
    return QVersionNumber(segments);
}

/*!
    Returns the name of the operating system, such as \c Ubuntu on Linux or
    \c Windows. Returns \c Unknown if it has none.
*/
QString QOperatingSystemVersion::name() const {
    switch (m_os) {
    case Windows:
        return QStringLiteral("Windows");
    case MacOS:
        return QStringLiteral("macOS");
    case IOS:
        return QStringLiteral("iOS");
    case TvOS:
        return QStringLiteral("tvOS");
    case WatchOS:
        return QStringLiteral("watchOS");
    case Android:
        return QStringLiteral("Android");
    case VisionOS:
        return QStringLiteral("visionOS");
    case Unknown:
        break;
    }
    return m_name.isEmpty() ? QStringLiteral("Unknown") : m_name;
}

/*!
    Returns \c true if the operating system family is one of \a types.
*/
bool QOperatingSystemVersion::isAnyOfType(std::initializer_list<OSType> types) const {
    for (OSType t : types)
        if (t == m_os)
            return true;
    return false;
}
