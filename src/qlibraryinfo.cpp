// MIT License - see LICENSE
#include <QtCore/qlibraryinfo.h>

/*!
    \class QLibraryInfo
    \brief The QLibraryInfo class reports how the Nt library was built.

    version() returns the version of the library the program is running
    against, which comes from the \c VERSION file at the root of the Nt
    repository:

    \code
    if (QLibraryInfo::version() < QVersionNumber(6, 1))
        qWarning() << "This program needs Nt 6.1 or newer";
    \endcode

    \differences
    The directories that path() returns are those of the installation that Nt was built for; there is no
    \c qt.conf to move them, and they do not follow the program if it is moved. platformPluginArguments()
    returns an empty list.
    \enddifferences

    \sa QVersionNumber
*/

/*!
    Returns the version of the Nt library. It is compiled into the library, so
    a program built against other headers still sees the version it runs with.

    \sa build()
*/
QVersionNumber QLibraryInfo::version() noexcept {
    return QVersionNumber(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
}

/*!
    Returns a line of text describing the build, including the version.

    \sa version()
*/
const char *QLibraryInfo::build() noexcept {
    return "Nt " QT_VERSION_STR " ("
#if defined(__clang__)
           "clang"
#elif defined(__GNUC__)
           "gcc"
#else
           "unknown compiler"
#endif
#ifdef NDEBUG
           ", release build"
#else
           ", debug build"
#endif
           ")";
}

/*!
    Returns \c true if the library was built with debugging enabled.
*/
bool QLibraryInfo::isDebugBuild() noexcept {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

/*!
    Returns \c true if Nt is a shared library rather than a static one.
*/
bool QLibraryInfo::isSharedBuild() noexcept {
#ifdef QT_SHARED
    return true;
#else
    return false;
#endif
}

// The runtime version, unlike QT_VERSION_STR, which is fixed at compile time.
const char *qVersion() noexcept { return QT_VERSION_STR; }

#ifndef NT_INSTALL_PREFIX
#define NT_INSTALL_PREFIX "/usr/local"
#define NT_INSTALL_LIBDIR "lib"
#define NT_INSTALL_BINDIR "bin"
#define NT_INSTALL_LIBEXECDIR "libexec"
#define NT_INSTALL_INCLUDEDIR "include"
#define NT_INSTALL_DATADIR "share"
#define NT_INSTALL_SYSCONFDIR "etc"
#endif

namespace {

// A directory of the installation: absolute as it is, or else under the prefix.
QString installDir(const char *dir) {
    QString d = QString::fromUtf8(dir);
    if (d.startsWith(QLatin1Char('/')))
        return d;
    return QString::fromUtf8(NT_INSTALL_PREFIX) + QLatin1Char('/') + d;
}

} // namespace

/*!
    \since 6.0
    Returns the directory \a p of the Nt installation, such as \c{/usr/lib} for \c LibrariesPath, or an empty
    string for a value that names no directory.

    \sa paths()
*/
QString QLibraryInfo::path(LibraryPath p) {
    switch (p) {
    case PrefixPath:
        return QString::fromUtf8(NT_INSTALL_PREFIX);
    case DocumentationPath:
        return installDir(NT_INSTALL_DATADIR) + QStringLiteral("/doc/nt6");
    case HeadersPath:
        return installDir(NT_INSTALL_INCLUDEDIR) + QStringLiteral("/nt6");
    case LibrariesPath:
        return installDir(NT_INSTALL_LIBDIR);
    case LibraryExecutablesPath:
        return installDir(NT_INSTALL_LIBEXECDIR);
    case BinariesPath:
        return installDir(NT_INSTALL_BINDIR);
    case PluginsPath:
        return installDir(NT_INSTALL_LIBDIR) + QStringLiteral("/nt6/plugins");
    case QmlImportsPath:
        return installDir(NT_INSTALL_LIBDIR) + QStringLiteral("/nt6/qml");
    case ArchDataPath:
        return installDir(NT_INSTALL_LIBDIR) + QStringLiteral("/nt6");
    case DataPath:
        return installDir(NT_INSTALL_DATADIR) + QStringLiteral("/nt6");
    case TranslationsPath:
        return installDir(NT_INSTALL_DATADIR) + QStringLiteral("/nt6/translations");
    case ExamplesPath:
        return installDir(NT_INSTALL_DATADIR) + QStringLiteral("/nt6/examples");
    case TestsPath:
        return installDir(NT_INSTALL_DATADIR) + QStringLiteral("/nt6/tests");
    case SettingsPath:
        return installDir(NT_INSTALL_SYSCONFDIR) + QStringLiteral("/xdg");
    }
    return QString();
}

/*!
    \since 6.8
    Returns the directories for \a p. Nt has one directory for each, the one path() returns.
*/
QStringList QLibraryInfo::paths(LibraryPath p) {
    const QString one = path(p);
    return one.isEmpty() ? QStringList() : QStringList{one};
}

/*!
    \since 6.0
    Returns the arguments that \c qt.conf gives the platform \a platformName. Nt reads no \c qt.conf, so the list is
    empty.
*/
QStringList QLibraryInfo::platformPluginArguments(const QString &platformName) {
    (void)platformName;
    return {};
}
