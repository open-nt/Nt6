// MIT License - see LICENSE

#include <QtCore/qstandardpaths.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <string>

/*
    Differences from Qt:
    - QCoreApplication has no applicationName()/organizationName() yet, so the
      application sub-directory is the executable's file name and no
      organisation directory is inserted.
    - Only the XDG base directory layout is supported (Linux/POSIX). XDG_*
      variables that are not absolute paths are ignored, as the spec requires.
    - displayName() returns the untranslated English names.
    - RuntimeLocation creates /tmp/runtime-<user> (mode 0700) when
      XDG_RUNTIME_DIR is unset, but does not verify its ownership.
*/

namespace {

QString envPath(const char *name) {
    const char *v = std::getenv(name);
    if (!v || *v != '/')
        return QString();
    return QDir::cleanPath(QString::fromUtf8(v));
}

QString homeDir() { return QDir::homePath(); }

QString dirFromEnv(const char *var, const char *homeRelative) {
    const QString v = envPath(var);
    return v.isEmpty() ? QDir::cleanPath(homeDir() + QChar(u'/') + QString::fromUtf8(homeRelative)) : v;
}

std::atomic<bool> g_testMode{false};

// In test mode the user directories move under ~/.qttest, so tests cannot touch real data.
QString testHome(const char *sub) {
    return QDir::cleanPath(homeDir() + QString::fromUtf8("/.qttest/") + QString::fromUtf8(sub));
}

QString dataHome() { return g_testMode ? testHome("share") : dirFromEnv("XDG_DATA_HOME", ".local/share"); }
QString configHome() { return g_testMode ? testHome("config") : dirFromEnv("XDG_CONFIG_HOME", ".config"); }
QString cacheHome() { return g_testMode ? testHome("cache") : dirFromEnv("XDG_CACHE_HOME", ".cache"); }
QString stateHome() { return g_testMode ? testHome("state") : dirFromEnv("XDG_STATE_HOME", ".local/state"); }

QStringList dirList(const char *var, const char *fallback) {
    const char *v = std::getenv(var);
    const QString raw = QString::fromUtf8(v && *v ? v : fallback);
    QStringList out;
    for (const QString &p : raw.split(QChar(u':'), Qt::SkipEmptyParts))
        if (p.startsWith(QChar(u'/')))
            out.append(QDir::cleanPath(p));
    return out;
}

QString appName() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0)
        return QString();
    return QFileInfo(QString::fromUtf8(buf, static_cast<qsizetype>(n))).fileName();
}

QString withApp(const QString &base) {
    const QString app = appName();
    return app.isEmpty() ? base : base + QChar(u'/') + app;
}

// Looks up a key such as XDG_MUSIC_DIR in ~/.config/user-dirs.dirs.
QString userDir(const char *key, const char *fallbackName) {
    const QString home = homeDir();
    QString result;
    std::ifstream in((configHome() + QString::fromUtf8("/user-dirs.dirs")).toStdString());
    std::string line;
    const std::string prefix = std::string(key) + "=";
    while (std::getline(in, line)) {
        std::size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line.compare(start, prefix.size(), prefix) != 0)
            continue;
        std::string value = line.substr(start + prefix.size());
        if (value.size() >= 2 && value.front() == '"') {
            const std::size_t close = value.find('"', 1);
            value = value.substr(1, close == std::string::npos ? std::string::npos : close - 1);
        }
        QString v = QString::fromStdString(value);
        if (v.startsWith(QString::fromUtf8("$HOME")))
            v = home + v.mid(5);
        if (v.startsWith(QChar(u'/')))
            result = QDir::cleanPath(v);
    }
    if (result.isEmpty() || result == home)
        result = QDir::cleanPath(home + QChar(u'/') + QString::fromUtf8(fallbackName));
    return result;
}

QString runtimeDir() {
    QString dir = envPath("XDG_RUNTIME_DIR");
    if (!dir.isEmpty())
        return dir;
    const passwd *pw = ::getpwuid(::getuid());
    const QString user = pw ? QString::fromUtf8(pw->pw_name) : QString::number(static_cast<qlonglong>(::getuid()));
    dir = QDir::tempPath() + QString::fromUtf8("/runtime-") + user;
    if (!QFileInfo(dir).isDir())
        ::mkdir(dir.toUtf8().constData(), 0700);
    return dir;
}

void appendUnique(QStringList &list, const QString &s) {
    if (!s.isEmpty() && !list.contains(s))
        list.append(s);
}

bool existsAsSpecified(const QString &path, QStandardPaths::LocateOptions options) {
    const QFileInfo fi(path);
    return (options & QStandardPaths::LocateDirectory) ? fi.isDir() : fi.isFile();
}

} // namespace

/*!
    \class QStandardPaths
    \brief The QStandardPaths class provides methods for accessing standard paths.

    The class locates the directories where a program should keep documents,
    configuration, cache and other data, following the XDG base directory
    specification. All functions are static.

    \code
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString exe = QStandardPaths::findExecutable("git");
    \endcode

    \differences
    Only the XDG layout of Linux and other POSIX systems is supported. The
    application sub-directory is the executable's file name, not the
    organization and application names.
    displayName() returns untranslated English names.

    On Zephyr the locations are still those of a Linux desktop and do not
    exist; choose paths on a mounted file system yourself. See \l{Nt on
    Zephyr}.
    \enddifferences
*/

/*!
    \enum QStandardPaths::StandardLocation

    Identifies a kind of standard location.

    \value DesktopLocation The user's desktop directory.
    \value DocumentsLocation The user's documents directory.
    \value FontsLocation The directory for fonts, below the XDG data home.
    \value ApplicationsLocation The directory for desktop entries, below the XDG data home.
    \value MusicLocation The user's music directory.
    \value MoviesLocation The user's videos directory.
    \value PicturesLocation The user's pictures directory.
    \value TempLocation The directory for temporary files, as returned by QDir::tempPath().
    \value HomeLocation The user's home directory.
    \value AppLocalDataLocation The application's data directory below the XDG data home. Same as AppDataLocation.
    \value CacheLocation The application's cache directory below the XDG cache home.
    \value GenericDataLocation The XDG data home, shared by all applications.
    \value RuntimeLocation The directory for runtime files such as sockets (\c XDG_RUNTIME_DIR).
    \value ConfigLocation The XDG configuration home. Same as GenericConfigLocation.
    \value DownloadLocation The user's download directory.
    \value GenericCacheLocation The XDG cache home, shared by all applications.
    \value GenericConfigLocation The XDG configuration home, shared by all applications.
    \value AppDataLocation The application's data directory below the XDG data home.
    \value AppConfigLocation The application's configuration directory below the XDG configuration home.
    \value PublicShareLocation The user's public share directory.
    \value TemplatesLocation The user's templates directory.
    \value StateLocation The application's state directory below the XDG state home.
    \value GenericStateLocation The XDG state home, shared by all applications.
*/

/*!
    \enum QStandardPaths::LocateOption

    Controls what locate() and locateAll() search for.

    \value LocateFile Return only regular files.
    \value LocateDirectory Return only directories.
*/

/*!
    \typedef QStandardPaths::LocateOptions

    A QFlags of LocateOption values.
*/

/*!
    Returns the directory where files of the given \a type should be written,
    or an empty string if the location cannot be determined. The directory is
    not created.

    \sa standardLocations(), locate()
*/
QString QStandardPaths::writableLocation(StandardLocation type) {
    switch (type) {
    case DesktopLocation: return userDir("XDG_DESKTOP_DIR", "Desktop");
    case DocumentsLocation: return userDir("XDG_DOCUMENTS_DIR", "Documents");
    case MusicLocation: return userDir("XDG_MUSIC_DIR", "Music");
    case MoviesLocation: return userDir("XDG_VIDEOS_DIR", "Videos");
    case PicturesLocation: return userDir("XDG_PICTURES_DIR", "Pictures");
    case DownloadLocation: return userDir("XDG_DOWNLOAD_DIR", "Downloads");
    case PublicShareLocation: return userDir("XDG_PUBLICSHARE_DIR", "Public");
    case TemplatesLocation: return userDir("XDG_TEMPLATES_DIR", "Templates");
    case FontsLocation: return dataHome() + QString::fromUtf8("/fonts");
    case ApplicationsLocation: return dataHome() + QString::fromUtf8("/applications");
    case TempLocation: return QDir::tempPath();
    case HomeLocation: return homeDir();
    case AppLocalDataLocation:
    case AppDataLocation: return withApp(dataHome());
    case CacheLocation: return withApp(cacheHome());
    case GenericDataLocation: return dataHome();
    case RuntimeLocation: return runtimeDir();
    case ConfigLocation:
    case GenericConfigLocation: return configHome();
    case GenericCacheLocation: return cacheHome();
    case AppConfigLocation: return withApp(configHome());
    case StateLocation: return withApp(stateHome());
    case GenericStateLocation: return stateHome();
    }
    return QString();
}

/*!
    Returns all directories of the given \a type, with the writable location
    first. The result does not contain empty or duplicate entries, and the
    directories are not checked for existence.

    \sa writableLocation(), locateAll()
*/
QStringList QStandardPaths::standardLocations(StandardLocation type) {
    QStringList dirs;
    appendUnique(dirs, writableLocation(type));
    if (g_testMode)
        return dirs; // only the test directories, not the system-wide ones
    const QStringList dataDirs = dirList("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    const QStringList configDirs = dirList("XDG_CONFIG_DIRS", "/etc/xdg");
    const QString app = appName();
    auto withAppSuffix = [&](const QString &base) { return app.isEmpty() ? base : base + QChar(u'/') + app; };
    switch (type) {
    case FontsLocation:
        appendUnique(dirs, QDir::cleanPath(homeDir() + QString::fromUtf8("/.fonts")));
        for (const QString &d : dataDirs)
            appendUnique(dirs, d + QString::fromUtf8("/fonts"));
        break;
    case ApplicationsLocation:
        for (const QString &d : dataDirs)
            appendUnique(dirs, d + QString::fromUtf8("/applications"));
        break;
    case AppLocalDataLocation:
    case AppDataLocation:
        for (const QString &d : dataDirs)
            appendUnique(dirs, withAppSuffix(d));
        break;
    case GenericDataLocation:
        for (const QString &d : dataDirs)
            appendUnique(dirs, d);
        break;
    case ConfigLocation:
    case GenericConfigLocation:
        for (const QString &d : configDirs)
            appendUnique(dirs, d);
        break;
    case AppConfigLocation:
        for (const QString &d : configDirs)
            appendUnique(dirs, withAppSuffix(d));
        break;
    default:
        break;
    }
    return dirs;
}

/*!
    \since 5.2
    Enables test mode if \a testMode is \c true, or disables it. In test
    mode the data, configuration, cache and state locations are
    sub-directories of \c{~/.qttest} (\c share, \c config, \c cache and
    \c state), and standardLocations() returns only the writable location,
    so that tests do not read or change the user's real files.

    \sa isTestModeEnabled()
*/
void QStandardPaths::setTestModeEnabled(bool testMode) { g_testMode = testMode; }

/*!
    \since 5.2
    Returns \c true if test mode is enabled.

    \sa setTestModeEnabled()
*/
bool QStandardPaths::isTestModeEnabled() { return g_testMode; }

/*!
    Finds the first entry named \a fileName in the standard locations of the
    given \a type that matches \a options. Returns its full path, or an empty
    string if there is none. If \a fileName is empty, the location directory
    itself is tested.

    \sa locateAll(), standardLocations()
*/
QString QStandardPaths::locate(StandardLocation type, const QString &fileName, LocateOptions options) {
    for (const QString &dir : standardLocations(type)) {
        const QString path = fileName.isEmpty() ? dir : dir + QChar(u'/') + fileName;
        if (existsAsSpecified(path, options))
            return path;
    }
    return QString();
}

/*!
    Finds every entry named \a fileName in the standard locations of the given
    \a type that matches \a options. Returns their full paths in search order.

    \sa locate(), standardLocations()
*/
QStringList QStandardPaths::locateAll(StandardLocation type, const QString &fileName, LocateOptions options) {
    QStringList found;
    for (const QString &dir : standardLocations(type)) {
        const QString path = fileName.isEmpty() ? dir : dir + QChar(u'/') + fileName;
        if (existsAsSpecified(path, options))
            found.append(path);
    }
    return found;
}

/*!
    Returns a human-readable, untranslated English name for the location
    \a type.
*/
QString QStandardPaths::displayName(StandardLocation type) {
    const char *name = "";
    switch (type) {
    case DesktopLocation: name = "Desktop"; break;
    case DocumentsLocation: name = "Documents"; break;
    case FontsLocation: name = "Fonts"; break;
    case ApplicationsLocation: name = "Applications"; break;
    case MusicLocation: name = "Music"; break;
    case MoviesLocation: name = "Movies"; break;
    case PicturesLocation: name = "Pictures"; break;
    case TempLocation: name = "Temporary Directory"; break;
    case HomeLocation: name = "Home"; break;
    case AppLocalDataLocation: name = "Application Data"; break;
    case CacheLocation: name = "Cache"; break;
    case GenericDataLocation: name = "Shared Data"; break;
    case RuntimeLocation: name = "Runtime"; break;
    case ConfigLocation: name = "Configuration"; break;
    case DownloadLocation: name = "Download"; break;
    case GenericCacheLocation: name = "Shared Cache"; break;
    case GenericConfigLocation: name = "Shared Configuration"; break;
    case AppDataLocation: name = "Application Data"; break;
    case AppConfigLocation: name = "Application Configuration"; break;
    case PublicShareLocation: name = "Public"; break;
    case TemplatesLocation: name = "Templates"; break;
    case StateLocation: name = "Application State"; break;
    case GenericStateLocation: name = "Shared State"; break;
    }
    return QString::fromUtf8(name);
}

/*!
    Finds an executable file named \a executableName and returns its absolute
    path, or an empty string if none is found. If \a executableName contains a
    slash it is tested directly. Otherwise each directory in \a paths is
    searched, or in the \c PATH environment variable if \a paths is empty.
*/
QString QStandardPaths::findExecutable(const QString &executableName, const QStringList &paths) {
    if (executableName.isEmpty())
        return QString();
    auto usable = [](const QString &candidate) {
        const QFileInfo fi(candidate);
        return fi.isFile() && fi.isExecutable();
    };
    if (executableName.contains(QChar(u'/'))) {
        const QFileInfo fi(executableName);
        return usable(executableName) ? fi.absoluteFilePath() : QString();
    }
    QStringList searchPaths = paths;
    if (searchPaths.isEmpty()) {
        const char *path = std::getenv("PATH");
        searchPaths = QString::fromUtf8(path ? path : "").split(QChar(u':'), Qt::SkipEmptyParts);
    }
    for (const QString &dir : searchPaths) {
        if (dir.isEmpty())
            continue;
        const QString candidate = QDir(dir).absoluteFilePath(executableName);
        if (usable(candidate))
            return QDir::cleanPath(candidate);
    }
    return QString();
}
