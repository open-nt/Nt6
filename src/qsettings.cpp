// MIT License - see LICENSE

#include <QtCore/qsettings.h>

#include "qsettings_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qlockfile.h>
#include <QtCore/qsavefile.h>
#include <QtCore/qstandardpaths.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace {

constexpr int kLockTimeoutMs = 10000;

// Orders keys by their code units, or ignoring case for a format that is case-insensitive.
struct KeyLess {
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    bool operator()(const QString &a, const QString &b) const {
        return cs == Qt::CaseSensitive ? a < b : a.compare(b, Qt::CaseInsensitive) < 0;
    }
};

using KeyMap = std::map<QString, QVariant, KeyLess>;
using KeySet = std::set<QString, KeyLess>;

// What is needed to tell if a file was replaced or modified since it was read.
struct FileSig {
    bool exists = false;
    qint64 size = 0;
    qint64 mtimeSec = 0;
    qint64 mtimeNsec = 0;
    quint64 inode = 0;
    quint64 device = 0;

    friend bool operator==(const FileSig &a, const FileSig &b) {
        return a.exists == b.exists && a.size == b.size && a.mtimeSec == b.mtimeSec && a.mtimeNsec == b.mtimeNsec &&
               a.inode == b.inode && a.device == b.device;
    }
    friend bool operator!=(const FileSig &a, const FileSig &b) { return !(a == b); }
};

FileSig statFile(const QString &path) {
    FileSig sig;
    struct stat st;
    if (::stat(path.toUtf8().constData(), &st) != 0)
        return sig;
    sig.exists = true;
    sig.size = static_cast<qint64>(st.st_size);
    // POSIX.1-2008 names; other platforms spell the modification time differently.
    sig.mtimeSec = static_cast<qint64>(st.st_mtim.tv_sec);
    sig.mtimeNsec = static_cast<qint64>(st.st_mtim.tv_nsec);
    sig.inode = static_cast<quint64>(st.st_ino);
    sig.device = static_cast<quint64>(st.st_dev);
    return sig;
}

// The state of one settings file, shared by every QSettings object of the process that uses it.
// Changes are kept in addedKeys and removedKeys until they are written, so that they can be applied
// on top of what another process wrote in the meantime.
struct ConfFile {
    ConfFile(QString fileName, Qt::CaseSensitivity sensitivity)
        : name(std::move(fileName)), cs(sensitivity), originalKeys(KeyLess{sensitivity}),
          addedKeys(KeyLess{sensitivity}), removedKeys(KeyLess{sensitivity}) {}

    KeyMap merged() const {
        KeyMap result = originalKeys;
        for (const QString &key : removedKeys)
            result.erase(key);
        for (const auto &entry : addedKeys)
            result[entry.first] = entry.second;
        return result;
    }

    QString name;
    Qt::CaseSensitivity cs;
    std::mutex mutex;
    int refs = 0;
    bool loaded = false;
    QSettings::Status readStatus = QSettings::NoError;
    FileSig sig;
    KeyMap originalKeys;
    KeyMap addedKeys;
    KeySet removedKeys;
};

struct CustomFormat {
    QString extension;
    QSettings::ReadFunc readFunc;
    QSettings::WriteFunc writeFunc;
    Qt::CaseSensitivity cs;
};

std::mutex g_globalMutex;
std::map<QString, ConfFile *> g_confFiles;
std::vector<CustomFormat> g_customFormats;
std::map<int, QString> g_paths;
std::atomic<int> g_defaultFormat{QSettings::NativeFormat};

constexpr int kMaxCustomFormats = 16;

int pathKey(QSettings::Format format, QSettings::Scope scope) { return (static_cast<int>(format) << 1) | static_cast<int>(scope); }

ConfFile *acquireConfFile(const QString &path, Qt::CaseSensitivity cs) {
    const QString name = QFileInfo(path).absoluteFilePath();
    const std::lock_guard<std::mutex> lock(g_globalMutex);
    ConfFile *&slot = g_confFiles[name];
    if (!slot)
        slot = new ConfFile(name, cs);
    ++slot->refs;
    return slot;
}

void releaseConfFile(ConfFile *file) {
    const std::lock_guard<std::mutex> lock(g_globalMutex);
    if (--file->refs == 0) {
        g_confFiles.erase(file->name);
        delete file;
    }
}

// Removes the leading slash and the repeated slashes of a key.
QString normalizedKey(const QString &key) {
    QString result;
    result.reserve(key.size());
    bool lastSlash = true;
    for (qsizetype i = 0; i < key.size(); ++i) {
        const QChar c = key.at(i);
        if (c == QChar(u'/')) {
            if (!lastSlash)
                result.append(c);
            lastSlash = true;
        } else {
            result.append(c);
            lastSlash = false;
        }
    }
    return result;
}

QStringList pathsFor(QSettings::Format format, QSettings::Scope scope) {
    {
        const std::lock_guard<std::mutex> lock(g_globalMutex);
        auto it = g_paths.find(pathKey(format, scope));
        if (it == g_paths.end())
            it = g_paths.find(pathKey(QSettings::IniFormat, scope));
        if (it != g_paths.end())
            return QStringList(it->second);
    }
    if (scope == QSettings::UserScope)
        return QStringList(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    if (!dirs.isEmpty())
        dirs.removeFirst();
    return dirs;
}

} // namespace

struct QSettings::Private {
    enum ChildSpec { AllKeys, ChildKeys, ChildGroups };

    // A group, or an array whose element is selected by setArrayIndex().
    struct Group {
        QString name;
        int num = -1; // 1-based index of the selected element; 0 before setArrayIndex(); -1 if not an array
        int maxNum = -1; // the array size seen so far; -1 if it was given to beginWriteArray()

        QString toString() const { return num > 0 ? name + QChar(u'/') + QString::number(num) : name; }
        bool isArray() const { return num != -1; }
    };

    Private(QSettings *owner, Format fmt, Scope sc, QString organization, QString application)
        : q(owner), format(fmt), scope(sc), orgName(std::move(organization)), appName(std::move(application)) {
        initFormat();
        QString org = orgName;
        if (org.isEmpty()) {
            setStatus(AccessError);
            org = QString::fromLatin1("Unknown Organization");
        }
        const QString slash = QString(QChar(u'/'));
        const QString appFile = org + slash + appName + extension;
        const QString orgFile = org + extension;
        const auto add = [this](const QString &dir, const QString &file) {
            const QString path = QDir::cleanPath(dir + QChar(u'/') + file);
            ConfFile *f = acquireConfFile(path, cs);
            if (std::find(confFiles.begin(), confFiles.end(), f) != confFiles.end())
                releaseConfFile(f);
            else
                confFiles.push_back(f);
        };
        if (scope == UserScope) {
            for (const QString &dir : pathsFor(format, UserScope)) {
                if (!appName.isEmpty())
                    add(dir, appFile);
                add(dir, orgFile);
            }
        }
        for (const QString &dir : pathsFor(format, SystemScope)) {
            if (!appName.isEmpty())
                add(dir, appFile);
            add(dir, orgFile);
        }
        initAccess();
    }

    Private(QSettings *owner, const QString &fileName, Format fmt)
        : q(owner), format(fmt), scope(UserScope) {
        initFormat();
        confFiles.push_back(acquireConfFile(fileName, cs));
        initAccess();
    }

    ~Private() {
        for (ConfFile *f : confFiles)
            releaseConfFile(f);
    }

    Q_DISABLE_COPY(Private)

    void initFormat() {
        extension = format == NativeFormat ? QString::fromLatin1(".conf") : QString::fromLatin1(".ini");
        if (format == NativeFormat || format == IniFormat) {
            readFunc = &NtSettings::readIniFile;
            writeFunc = &NtSettings::writeIniFile;
        } else if (format >= CustomFormat1) {
            const std::lock_guard<std::mutex> lock(g_globalMutex);
            const auto index = static_cast<std::size_t>(format - CustomFormat1);
            if (index < g_customFormats.size()) {
                const CustomFormat &info = g_customFormats[index];
                extension = info.extension;
                readFunc = info.readFunc;
                writeFunc = info.writeFunc;
                cs = info.cs;
            }
        }
    }

    void initAccess() {
        if (format > IniFormat && !readFunc)
            setStatus(AccessError);
        sync();
    }

    void setStatus(Status s) const {
        if (s == NoError || status == NoError)
            status = s;
    }

    QString actualKey(const QString &key) const { return groupPrefix + normalizedKey(key); }

    void beginGroupOrArray(const Group &group) {
        groupStack.push_back(group);
        if (!group.name.isEmpty())
            groupPrefix += group.name + QChar(u'/');
    }

    void rebuildPrefix() {
        groupPrefix.clear();
        for (const Group &g : groupStack) {
            const QString name = g.toString();
            if (!name.isEmpty())
                groupPrefix += name + QChar(u'/');
        }
    }

    void requestUpdate() {
        if (!updateRequest) {
            updateRequest = true;
            QCoreApplication::postEvent(q, new QEvent(QEvent::UpdateRequest));
        }
    }

    void update() {
        flush();
        updateRequest = false;
    }

    void flush() {
        sync();
        pendingChanges = false;
    }

    bool get(const QString &key, QVariant *value) const {
        for (ConfFile *f : confFiles) {
            const std::lock_guard<std::mutex> lock(f->mutex);
            auto it = f->addedKeys.find(key);
            if (it != f->addedKeys.end()) {
                if (value)
                    *value = it->second;
                return true;
            }
            it = f->originalKeys.find(key);
            if (it != f->originalKeys.end() && f->removedKeys.find(key) == f->removedKeys.end()) {
                if (value)
                    *value = it->second;
                return true;
            }
            if (!fallbacks)
                break;
        }
        return false;
    }

    void set(const QString &key, const QVariant &value) {
        ConfFile *f = confFiles.front();
        const std::lock_guard<std::mutex> lock(f->mutex);
        f->addedKeys[key] = value;
        f->removedKeys.erase(key);
        pendingChanges = true;
    }

    static void eraseWithPrefix(KeyMap &map, const QString &prefix, Qt::CaseSensitivity cs) {
        auto it = map.lower_bound(prefix);
        while (it != map.end() && it->first.startsWith(prefix, cs))
            it = map.erase(it);
    }

    void remove(const QString &key) {
        ConfFile *f = confFiles.front();
        const std::lock_guard<std::mutex> lock(f->mutex);
        pendingChanges = true;
        if (key.isEmpty()) {
            clearLocked(*f);
            return;
        }
        const QString prefix = key + QChar(u'/');
        eraseWithPrefix(f->addedKeys, prefix, cs);
        f->addedKeys.erase(key);
        for (auto it = f->originalKeys.lower_bound(prefix); it != f->originalKeys.end() && it->first.startsWith(prefix, cs);
             ++it)
            f->removedKeys.insert(it->first);
        if (f->originalKeys.find(key) != f->originalKeys.end())
            f->removedKeys.insert(key);
    }

    static void clearLocked(ConfFile &f) {
        f.addedKeys.clear();
        for (const auto &entry : f.originalKeys)
            f.removedKeys.insert(entry.first);
    }

    void clear() {
        ConfFile *f = confFiles.front();
        const std::lock_guard<std::mutex> lock(f->mutex);
        clearLocked(*f);
        pendingChanges = true;
    }

    QStringList children(ChildSpec spec) const {
        KeySet result((KeyLess{cs}));
        const QString &prefix = groupPrefix;
        for (ConfFile *f : confFiles) {
            {
                const std::lock_guard<std::mutex> lock(f->mutex);
                const auto visit = [&](const QString &key) {
                    const QString rest = key.mid(prefix.size());
                    if (rest.isEmpty())
                        return;
                    const qsizetype slash = rest.indexOf(QChar(u'/'));
                    if (spec == AllKeys || (slash < 0 && spec == ChildKeys))
                        result.insert(rest);
                    else if (slash >= 0 && spec == ChildGroups)
                        result.insert(rest.left(slash));
                };
                for (auto it = f->originalKeys.lower_bound(prefix);
                     it != f->originalKeys.end() && it->first.startsWith(prefix, cs); ++it) {
                    if (f->removedKeys.find(it->first) == f->removedKeys.end())
                        visit(it->first);
                }
                for (auto it = f->addedKeys.lower_bound(prefix);
                     it != f->addedKeys.end() && it->first.startsWith(prefix, cs); ++it)
                    visit(it->first);
            }
            if (!fallbacks)
                break;
        }
        QStringList list;
        for (const QString &s : result)
            list.append(s);
        return list;
    }

    void sync() {
        for (ConfFile *f : confFiles) {
            const std::lock_guard<std::mutex> lock(f->mutex);
            syncConfFile(*f);
        }
    }

    void loadConfFile(ConfFile &f, const FileSig &sig) {
        f.originalKeys.clear();
        f.loaded = true;
        f.sig = sig;
        f.readStatus = NoError;
        if (!sig.exists)
            return;
        QFile file(f.name);
        if (QFileInfo(f.name).isDir() || !file.open(QIODevice::ReadOnly)) {
            f.readStatus = AccessError;
            return;
        }
        SettingsMap map;
        if (!readFunc(file, map))
            f.readStatus = FormatError;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            f.originalKeys[it.key()] = it.value();
    }

    // The caller holds f.mutex.
    void syncConfFile(ConfFile &f) {
        if (!readFunc)
            return;
        const bool dirty = !f.addedKeys.empty() || !f.removedKeys.empty();
        if (!dirty) {
            const FileSig sig = statFile(f.name);
            if (!f.loaded || sig != f.sig)
                loadConfFile(f, sig);
            if (f.readStatus != NoError)
                setStatus(f.readStatus);
            return;
        }
        if (!writeFunc) {
            setStatus(AccessError);
            return;
        }

        // Another process may be writing at the same time: hold the lock while merging and writing.
        QDir().mkpath(QFileInfo(f.name).absolutePath());
        QLockFile lock(f.name + QString::fromLatin1(".lock"));
        if (!lock.tryLock(kLockTimeoutMs)) {
            setStatus(AccessError);
            return;
        }
        const FileSig sig = statFile(f.name);
        if (!f.loaded || sig != f.sig)
            loadConfFile(f, sig);
        if (f.readStatus != NoError)
            setStatus(f.readStatus);

        const KeyMap merged = f.merged();
        SettingsMap out;
        for (const auto &entry : merged)
            out.insert(entry.first, entry.second);

        QSaveFile file(f.name);
        file.setDirectWriteFallback(!atomicSync);
        if (!file.open(QIODevice::WriteOnly)) {
            setStatus(AccessError);
            return;
        }
        if (!writeFunc(file, out)) {
            file.cancelWriting();
            setStatus(FormatError);
            return;
        }
        if (!file.commit()) {
            setStatus(AccessError);
            return;
        }
        f.originalKeys = merged;
        f.addedKeys.clear();
        f.removedKeys.clear();
        f.sig = statFile(f.name);
        f.loaded = true;
        f.readStatus = NoError;
    }

    bool isWritable() const {
        if (!readFunc || !writeFunc || confFiles.empty())
            return false;
        const QString &name = confFiles.front()->name;
        const QByteArray path = name.toUtf8();
        if (::access(path.constData(), F_OK) == 0)
            return ::access(path.constData(), W_OK) == 0;
        // The file would be created in the nearest directory that exists.
        QString dir = QFileInfo(name).absolutePath();
        while (!dir.isEmpty() && !QFileInfo(dir).exists()) {
            const QString parent = QFileInfo(dir).absolutePath();
            if (parent == dir)
                return false;
            dir = parent;
        }
        return QFileInfo(dir).isDir() && ::access(dir.toUtf8().constData(), W_OK | X_OK) == 0;
    }

    QSettings *q;
    Format format;
    Scope scope;
    QString orgName;
    QString appName;
    QString extension;
    ReadFunc readFunc = nullptr;
    WriteFunc writeFunc = nullptr;
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    std::vector<ConfFile *> confFiles;
    bool fallbacks = true;
    bool atomicSync = true;
    bool pendingChanges = false;
    bool updateRequest = false;
    mutable Status status = NoError;
    QString groupPrefix;
    std::vector<Group> groupStack;
};

/*!
    \class QSettings
    \brief The QSettings class provides persistent platform-independent application settings.

    Users expect applications to remember their settings (window sizes and positions, options,
    and so on) across sessions. QSettings stores them in a file, and a setting is a key and a
    QVariant. Keys are strings that can be grouped with \c{/}, like the path of a file.

    \code
    QCoreApplication::setOrganizationName("MySoft");
    QCoreApplication::setApplicationName("Star Runner");

    QSettings settings;
    settings.setValue("editor/wrapMargin", 68);
    int margin = settings.value("editor/wrapMargin", 80).toInt();
    \endcode

    \section1 Locations

    QSettings(const QString &organization, const QString &application) and the constructors
    that depend on QCoreApplication use these files on Linux, in the order in which they are
    searched. \c{$XDG_CONFIG_HOME} defaults to \c{~/.config}, and \c{$XDG_CONFIG_DIRS} to
    \c{/etc/xdg}. The extension is \c{.conf} for \c NativeFormat and \c{.ini} for \c IniFormat.

    \list 1
    \li \c{$XDG_CONFIG_HOME/MySoft/Star Runner.conf} (user scope only)
    \li \c{$XDG_CONFIG_HOME/MySoft.conf} (user scope only)
    \li \c{/etc/xdg/MySoft/Star Runner.conf}
    \li \c{/etc/xdg/MySoft.conf}
    \endlist

    A value that is not found in the first file is looked up in the next ones, unless
    setFallbacksEnabled() turns that off. Settings are always written to the first file; the
    directories are created as needed. The organization-wide files let several applications share
    settings. If the organization name is empty, status() is \c AccessError and the files are
    placed under the name "Unknown Organization". A QSettings can also use one explicit file
    (QSettings(const QString &fileName, Format format)); the file name must be given in a format
    that can be read, as \c NativeFormat and \c IniFormat are both the INI format on Linux.
    setPath() changes the directories, and setDefaultFormat() the format of the constructors that
    take none.

    \section1 Groups and arrays

    beginGroup() and endGroup() add a prefix to every key. beginWriteArray() and beginReadArray()
    store lists of structures as \c{prefix/1/key}, \c{prefix/2/key} and so on, with
    \c{prefix/size}; setArrayIndex() selects the element, counting from 0.

    \code
    int size = settings.beginReadArray("logins");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString user = settings.value("userName").toString();
    }
    settings.endArray();
    \endcode

    \section1 Writing

    Changes are kept in memory, so that every QSettings object of the process that uses the same
    file sees them at once. They are written to the file when the event loop runs, when sync() is
    called, and when the object is destroyed. The write takes the lock file \c{<file>.lock} with
    QLockFile, reads the file again if another process changed it, applies the changes made by
    this process on top, and replaces the file with QSaveFile, so that readers never see a partial
    file and writers do not lose each other's changes. sync() also reads the file again if it
    changed.

    \section1 The INI format

    \code
    [General]
    size=@Size(800 600)
    title=Star Runner

    [editor]
    wrapMargin=68
    recent=a.txt, b.txt
    \endcode

    Keys without a group are in the section \c{[General]}; a key \c{a/b/c} is the key \c{c} in the
    section called \c{a/b}, which is written with a backslash in place of the slash. Characters in
    group and key names other than letters, digits and \c{_-.} are
    written as \c{%XX} (or \c{%UXXXX}), so \c{My Group} becomes \c{My%20Group}. Values of
    type QString, the integer types, \c bool and the floating-point types are written as text,
    and are read back as QString: use \c toInt() and the like. A string that contains \c{,}, \c{;}
    or \c{=}, or begins or ends with a blank, is quoted; backslashes, double quotes and control
    characters are escaped, and non-ASCII characters are UTF-8. A QStringList is written as comma-separated
    items and read back as a QStringList when it has several, an empty one is written as
    \c{@Invalid()} and read back as an invalid QVariant, and a QVariantList is written the same way
    but is read as a QStringList, or as a QVariantList when an item has a special form. A
    QByteArray is \c{@ByteArray(...)}, QPoint, QSize and QRect are \c{@Point(x y)},
    \c{@Size(w h)} and \c{@Rect(x y w h)}, a QDateTime is \c{@DateTime(...)}, and any other type
    is \c{@Variant(...)} with the bytes of its QDataStream form (version \c Qt_4_0), with unprintable
    bytes escaped. A string that begins with \c{@} gets a second one. The file is read as Qt
    reads it: a line that begins with \c{;} is a comment, and so is the rest of a line after a
    \c{;} that is not quoted; a backslash at the end of a line continues it; blanks around keys,
    values and list items are ignored.

    Keys and sections are case-sensitive in the INI format. A section named \c General (in any
    case) holds the keys without a group; a group called \c General is written as \c{[%General]}.
    Settings are written sorted, with \c General first, so the output is deterministic.

    \section1 Custom formats

    registerFormat() adds a format that is read and written by functions of your own, using a
    SettingsMap of full keys:

    \code
    static bool readXml(QIODevice &device, QSettings::SettingsMap &map);
    static bool writeXml(QIODevice &device, const QSettings::SettingsMap &map);

    const QSettings::Format XmlFormat = QSettings::registerFormat("xml", readXml, writeXml);
    QSettings settings(XmlFormat, QSettings::UserScope, "MySoft", "Star Runner");
    \endcode

    \section1 Thread safety

    QSettings objects are reentrant: different objects, in different threads or different
    processes, may use the same file. Each object must be used from one thread at a time.

    \differences
    The keys and group names are \c{const QString &}, as Nt has no QAnyStringView. The registry
    formats \c Registry32Format and \c Registry64Format are only enumerators: on Linux they make
    status() return \c AccessError, as does any unregistered custom format. \c NativeFormat is
    the INI format, and the macOS property lists are not supported. The keys of the INI format
    are case-sensitive on every platform. A line that begins with \c{#} in an INI file is also
    taken for a comment. The default directories are read from the environment (\c XDG_CONFIG_HOME
    and \c XDG_CONFIG_DIRS) each time a QSettings object is constructed, not once. There is no
    \c setSystemIniPath() or \c setUserIniPath(), which Qt 6 removed. The sections of an INI file
    are read at once, not when first used. A \c{@Variant()} value is only restored if the
    stream operators of the type are registered. Comments and the order of the keys of an INI file
    are not preserved when it is rewritten. A QSettings object is not thread-safe itself, and
    \c QSettings::value() on a file that another process is replacing sees either the old or the
    new file, never part of both.
    \enddifferences

    \sa QVariant, QSaveFile, QLockFile, QStandardPaths
*/

/*!
    Constructs a QSettings object for accessing the settings of the application called
    \a application from the organization called \a organization, with the given \a parent. The
    settings are those of the current user, in the default format.

    If \a application is empty, only the organization-wide settings are accessed.

    \sa setDefaultFormat(), setPath(), fileName()
*/
QSettings::QSettings(const QString &organization, const QString &application, QObject *parent)
    : QSettings(defaultFormat(), UserScope, organization, application, parent) {}

/*!
    Constructs a QSettings object for accessing the settings of the application called
    \a application from the organization called \a organization, in the given \a scope, with the
    given \a parent.

    \sa setDefaultFormat()
*/
QSettings::QSettings(Scope scope, const QString &organization, const QString &application, QObject *parent)
    : QSettings(defaultFormat(), scope, organization, application, parent) {}

/*!
    Constructs a QSettings object for accessing the settings of the application called
    \a application from the organization called \a organization, in the given \a format and
    \a scope, with the given \a parent.

    If the \a format is not usable, status() returns \c AccessError and nothing can be read or
    written.

    \sa Format, Scope
*/
QSettings::QSettings(Format format, Scope scope, const QString &organization, const QString &application,
                     QObject *parent)
    : QObject(parent), p(new Private(this, format, scope, organization, application)) {}

/*!
    Constructs a QSettings object for accessing the settings stored in the file called
    \a fileName, with the given \a parent. If the file does not exist, it is created when the
    settings are written. The \a format is the way the file is read and written;
    \c NativeFormat and \c IniFormat are the same here.

    \sa fileName()
*/
QSettings::QSettings(const QString &fileName, Format format, QObject *parent)
    : QObject(parent), p(new Private(this, fileName, format)) {}

/*!
    Constructs a QSettings object for accessing the settings of the application, with the given
    \a parent. The organization name is QCoreApplication::organizationName(), or
    QCoreApplication::organizationDomain() if it is empty, and the application name is
    QCoreApplication::applicationName(). The settings are those of the current user, in the
    default format.

    \sa setDefaultFormat()
*/
QSettings::QSettings(QObject *parent) : QSettings(UserScope, parent) {}

/*!
    Constructs a QSettings object for accessing the settings of the application in the given
    \a scope, with the given \a parent. The organization and application names are those of
    QCoreApplication.

    \sa QSettings()
*/
QSettings::QSettings(Scope scope, QObject *parent)
    : QSettings(defaultFormat(), scope,
                QCoreApplication::organizationName().isEmpty() ? QCoreApplication::organizationDomain()
                                                               : QCoreApplication::organizationName(),
                QCoreApplication::applicationName(), parent) {}

/*!
    Writes any unsaved changes to permanent storage, and destroys the object.

    \sa sync()
*/
QSettings::~QSettings() {
    if (p->pendingChanges)
        p->flush();
}

/*!
    Removes all entries in the primary location associated to this QSettings object. Entries in
    fallback locations are not removed.

    If you only want to remove the entries in the current group(), use remove("") instead.

    \sa remove(), setFallbacksEnabled()
*/
void QSettings::clear() {
    p->clear();
    p->requestUpdate();
}

/*!
    Writes any unsaved changes to permanent storage, and reloads any settings that have been
    changed in the meantime by another application.

    This function is called automatically from the event loop after settings have been set, and
    from the destructor, so you should not normally need to call it.

    \sa status()
*/
void QSettings::sync() {
    p->sync();
    p->pendingChanges = false;
}

/*!
    Returns a status code indicating the first error that was met by QSettings, or
    \c QSettings::NoError if no error occurred.

    \sa Status
*/
QSettings::Status QSettings::status() const { return p->status; }

/*!
    Returns \c true if QSettings is only allowed to perform atomic saving and reloading
    (synchronization) of the settings; otherwise returns \c false. The default is \c true.

    \sa setAtomicSyncRequired()
*/
bool QSettings::isAtomicSyncRequired() const { return p->atomicSync; }

/*!
    Configures whether QSettings is required to use atomic saving and reloading (synchronization)
    of the settings, according to \a enable.

    If it is not required, and the directory of the file cannot be written (so that no
    temporary file can be created), the file is written in place, which is not atomic.

    \sa isAtomicSyncRequired()
*/
void QSettings::setAtomicSyncRequired(bool enable) { p->atomicSync = enable; }

/*!
    Appends \a prefix to the current group.

    The current group is automatically prepended to all keys specified to QSettings. In addition,
    query functions such as childGroups(), childKeys() and allKeys() are based on the group. By
    default, no group is set.

    Groups are useful to avoid typing in the same setting paths over and over. For example:

    \code
    settings.beginGroup("mainwindow");
    settings.setValue("size", win->size());
    settings.setValue("pos", win->pos());
    settings.endGroup();
    \endcode

    \sa endGroup(), group()
*/
void QSettings::beginGroup(const QString &prefix) { p->beginGroupOrArray(Private::Group{normalizedKey(prefix)}); }

/*!
    Resets the group to what it was before the corresponding beginGroup() call. A warning is
    printed if there is no matching beginGroup().

    \sa beginGroup(), group()
*/
void QSettings::endGroup() {
    if (p->groupStack.empty()) {
        qWarning("QSettings::endGroup: No matching beginGroup()");
        return;
    }
    const bool isArray = p->groupStack.back().isArray();
    p->groupStack.pop_back();
    p->rebuildPrefix();
    if (isArray)
        qWarning("QSettings::endGroup: Expected endArray() instead");
}

/*!
    Returns the current group, without a trailing slash.

    \sa beginGroup(), endGroup()
*/
QString QSettings::group() const { return p->groupPrefix.left(p->groupPrefix.size() - 1); }

/*!
    Adds \a prefix to the current group and starts reading from an array. Returns the size of the
    array.

    \code
    int size = settings.beginReadArray("logins");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString user = settings.value("userName").toString();
    }
    settings.endArray();
    \endcode

    Use beginWriteArray() to write the array in the first place.

    \sa beginWriteArray(), endArray(), setArrayIndex()
*/
int QSettings::beginReadArray(const QString &prefix) {
    p->beginGroupOrArray(Private::Group{normalizedKey(prefix), 0, -1});
    return value(QString::fromLatin1("size")).toInt();
}

/*!
    Adds \a prefix to the current group and starts writing an array of size \a size. If \a size
    is -1 (the default), it is automatically determined based on the indexes of the entries
    written.

    If you have many occurrences of a certain set of keys, you can use arrays to make your life
    easier. For example, let's suppose that you want to save a list of users consisting of user
    name and password fields. You could then write:

    \code
    struct Login {
        QString userName;
        QString password;
    };
    QList<Login> logins;

    QSettings settings;
    settings.beginWriteArray("logins");
    for (int i = 0; i < logins.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("userName", logins.at(i).userName);
        settings.setValue("password", logins.at(i).password);
    }
    settings.endArray();
    \endcode

    The elements are stored as \c{logins/1/userName} and so on, and the size as \c{logins/size}.

    \sa beginReadArray(), endArray(), setArrayIndex()
*/
void QSettings::beginWriteArray(const QString &prefix, int size) {
    p->beginGroupOrArray(Private::Group{normalizedKey(prefix), 0, size < 0 ? 0 : -1});
    if (size < 0)
        remove(QString::fromLatin1("size"));
    else
        setValue(QString::fromLatin1("size"), size);
}

/*!
    Closes the array that was started using beginReadArray() or beginWriteArray(). A warning is
    printed if there is no matching begin.

    \sa beginReadArray(), beginWriteArray()
*/
void QSettings::endArray() {
    if (p->groupStack.empty()) {
        qWarning("QSettings::endArray: No matching beginArray()");
        return;
    }
    const Private::Group group = p->groupStack.back();
    p->groupStack.pop_back();
    p->rebuildPrefix();
    if (group.maxNum != -1)
        setValue(group.name + QString::fromLatin1("/size"), group.maxNum);
    if (!group.isArray())
        qWarning("QSettings::endArray: Expected endGroup() instead");
}

/*!
    Sets the current array index to \a i. Calls to functions such as setValue(), value(),
    remove() and contains() act on the array entry at that index.

    You must call beginReadArray() or beginWriteArray() before you can call this function. Note
    that the indexes in the file start at 1, not 0.

    \sa beginReadArray(), beginWriteArray()
*/
void QSettings::setArrayIndex(int i) {
    if (p->groupStack.empty() || !p->groupStack.back().isArray()) {
        qWarning("QSettings::setArrayIndex: Missing beginArray()");
        return;
    }
    Private::Group &top = p->groupStack.back();
    top.num = qMax(i, 0) + 1;
    if (top.maxNum != -1 && top.num > top.maxNum)
        top.maxNum = top.num;
    p->rebuildPrefix();
}

/*!
    Returns a list of all keys, including subkeys, that can be read using the QSettings object.

    Example:

    \code
    settings.setValue("fridge/color", QColor(Qt::white));
    settings.setValue("fridge/size", QSize(32, 96));
    settings.setValue("sofa", true);
    settings.setValue("tv", false);

    QStringList keys = settings.allKeys();
    // keys: ["fridge/color", "fridge/size", "sofa", "tv"]
    \endcode

    If a group is set using beginGroup(), only the keys in the group are returned, without the
    group prefix. The list is sorted.

    \sa childGroups(), childKeys()
*/
QStringList QSettings::allKeys() const { return p->children(Private::AllKeys); }

/*!
    Returns a list of all top-level keys that can be read using the QSettings object. The list
    is sorted.

    \sa allKeys(), childGroups()
*/
QStringList QSettings::childKeys() const { return p->children(Private::ChildKeys); }

/*!
    Returns a list of all key top-level groups that contain keys that can be read using the
    QSettings object. The list is sorted.

    \sa childKeys(), allKeys()
*/
QStringList QSettings::childGroups() const { return p->children(Private::ChildGroups); }

/*!
    Returns \c true if settings can be written using this QSettings object; returns \c false
    otherwise. This is the case if the primary file can be written, or created, and the format
    can write.

    \sa status(), sync()
*/
bool QSettings::isWritable() const { return p->isWritable(); }

/*!
    Sets the value of setting \a key to \a value. If the \a key already exists, the previous value
    is overwritten.

    Keys are case-sensitive in the INI format, but not in every custom format or in Qt on
    some platforms: to stay portable, always refer to a key with the same case and avoid keys
    that differ only by case. An empty \a key is refused with a warning.

    \sa value(), remove(), contains()
*/
void QSettings::setValue(const QString &key, const QVariant &value) {
    if (key.isEmpty()) {
        qWarning("QSettings::setValue: Empty key passed");
        return;
    }
    p->set(p->actualKey(key), value);
    p->requestUpdate();
}

/*!
    Returns the value for setting \a key, or an invalid QVariant if the setting does not exist.

    \sa setValue(), contains()
*/
QVariant QSettings::value(const QString &key) const { return value(key, QVariant()); }

/*!
    \overload

    Returns the value for setting \a key. If the setting does not exist, returns \a defaultValue.

    If no default value is specified, a default QVariant is returned.

    The settings are looked up in the primary file and then in the fallback ones, unless
    fallbacksEnabled() is \c false. A value read from a file is a QString, a QStringList, a
    QByteArray or whatever its \c{@Variant()} form encodes; convert it with \c toInt() and the
    like.

    \sa setValue(), contains(), remove()
*/
QVariant QSettings::value(const QString &key, const QVariant &defaultValue) const {
    if (key.isEmpty()) {
        qWarning("QSettings::value: Empty key passed");
        return QVariant();
    }
    QVariant result = defaultValue;
    p->get(p->actualKey(key), &result);
    return result;
}

/*!
    Removes the setting \a key and any sub-settings of \a key.

    Example:

    \code
    settings.setValue("ape", 0);
    settings.setValue("monkey", 1);
    settings.setValue("monkey/sea", 2);
    settings.setValue("monkey/doe", 4);

    settings.remove("monkey");
    QStringList keys = settings.allKeys();
    // keys: ["ape"]
    \endcode

    Be aware that if one of the fallback locations contains a setting with the same key, that
    setting will be visible after calling remove().

    If \a key is an empty string, all keys in the current group() are removed.

    \sa clear(), setValue(), value(), contains()
*/
void QSettings::remove(const QString &key) {
    QString theKey = normalizedKey(key);
    if (theKey.isEmpty())
        theKey = group();
    else
        theKey.prepend(p->groupPrefix);
    p->remove(theKey);
    p->requestUpdate();
}

/*!
    Returns \c true if there exists a setting called \a key; returns \c false otherwise.

    If a group is set using beginGroup(), \a key is taken to be relative to that group.

    \sa value(), setValue()
*/
bool QSettings::contains(const QString &key) const {
    if (key.isEmpty())
        return false;
    return p->get(p->actualKey(key), nullptr);
}

/*!
    Sets whether fallbacks are enabled to \a b.

    By default, fallbacks are enabled: a key that is not in the primary file is looked up in the
    organization-wide and system-wide files.

    \sa fallbacksEnabled()
*/
void QSettings::setFallbacksEnabled(bool b) { p->fallbacks = b; }

/*!
    Returns \c true if fallbacks are enabled; returns \c false otherwise.

    By default, fallbacks are enabled.

    \sa setFallbacksEnabled()
*/
bool QSettings::fallbacksEnabled() const { return p->fallbacks; }

/*!
    Returns the path where settings written using this QSettings object are stored: the absolute
    path of the primary file.

    \sa isWritable(), format()
*/
QString QSettings::fileName() const { return p->confFiles.empty() ? QString() : p->confFiles.front()->name; }

/*!
    Returns the format used for storing the settings.

    \sa defaultFormat(), fileName(), scope()
*/
QSettings::Format QSettings::format() const { return p->format; }

/*!
    Returns the scope used for storing the settings.

    \sa format(), organizationName(), applicationName()
*/
QSettings::Scope QSettings::scope() const { return p->scope; }

/*!
    Returns the organization name used for storing the settings, as given to the constructor. It
    is empty for a QSettings that uses a file name.

    \sa QCoreApplication::organizationName(), format(), scope(), applicationName()
*/
QString QSettings::organizationName() const { return p->orgName; }

/*!
    Returns the application name used for storing the settings, as given to the constructor. It
    is empty for a QSettings that uses a file name.

    \sa QCoreApplication::applicationName(), format(), scope(), organizationName()
*/
QString QSettings::applicationName() const { return p->appName; }

/*!
    Sets the default file format to the given \a format, which is used for storing settings for
    the QSettings(const QString &organization, const QString &application, QObject *parent)
    constructor. If no default format is set, \c QSettings::NativeFormat is used.

    \sa defaultFormat(), format()
*/
void QSettings::setDefaultFormat(Format format) { g_defaultFormat.store(static_cast<int>(format)); }

/*!
    Returns default file format used for storing settings for the QSettings(const QString
    &organization, const QString &application, QObject *parent) constructor. If no default
    format is set, \c QSettings::NativeFormat is used.

    \sa setDefaultFormat()
*/
QSettings::Format QSettings::defaultFormat() { return static_cast<Format>(g_defaultFormat.load()); }

/*!
    Sets the path used for storing settings for the given \a format and \a scope, to \a path.
    The \a format can be a custom format.

    Without a path set, the user scope uses \c{$XDG_CONFIG_HOME} (or \c{~/.config}) and the system
    scope uses the directories of \c{$XDG_CONFIG_DIRS} (or \c{/etc/xdg}). A path set for
    \c IniFormat is also used by the other formats that have none of their own.

    This function does not affect QSettings objects that already exist.

    \sa QSettings()
*/
void QSettings::setPath(Format format, Scope scope, const QString &path) {
    const std::lock_guard<std::mutex> lock(g_globalMutex);
    g_paths[pathKey(format, scope)] = path;
}

/*!
    Registers a custom storage format. On success, returns a special Format value that can then
    be passed to the QSettings constructors. On failure, returns \c InvalidFormat: at most 16
    custom formats can be registered.

    The \a extension is the suffix of the files of the format, without the dot. The \a readFunc
    and \a writeFunc read and write a file as a SettingsMap of full keys; they return \c true on
    success, and the QSettings object reports \c FormatError otherwise. \a caseSensitivity tells
    whether the keys of the format are case-sensitive. The functions may be called from any
    thread, at any time, with the file locked against other writers.

    \sa setPath()
*/
QSettings::Format QSettings::registerFormat(const QString &extension, ReadFunc readFunc, WriteFunc writeFunc,
                                            Qt::CaseSensitivity caseSensitivity) {
    const std::lock_guard<std::mutex> lock(g_globalMutex);
    if (g_customFormats.size() >= static_cast<std::size_t>(kMaxCustomFormats))
        return InvalidFormat;
    g_customFormats.push_back({QString(QChar(u'.')) + extension, readFunc, writeFunc, caseSensitivity});
    return static_cast<Format>(CustomFormat1 + static_cast<int>(g_customFormats.size()) - 1);
}

/*!
    \reimp

    Writes the unsaved changes when the object receives the update request that it posts to
    itself after a change.
*/
bool QSettings::event(QEvent *event) {
    if (event->type() == QEvent::UpdateRequest) {
        p->update();
        return true;
    }
    return QObject::event(event);
}

#include "moc_qsettings.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
