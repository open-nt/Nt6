// MIT License - see LICENSE
#include <QtCore/qlibrary.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qstringlist.h>

#include <map>
#include <mutex>

#ifndef _WIN32
#  include <dlfcn.h>
#endif

/*!
    \class QLibrary
    \brief The QLibrary class loads shared libraries at runtime.

    A QLibrary names a shared library, loads it into the process, and finds the functions that
    the library exports by their names:

    \code
    QLibrary library(u"mylib"_s);
    using Average = int (*)(int, int);
    if (auto average = reinterpret_cast<Average>(library.resolve("average")))
        qDebug() << average(4, 8);
    else
        qWarning() << library.errorString();
    \endcode

    The file name may leave out what differs between platforms: for \c mylib, \c libmylib.so is
    tried as well, and with a version \c libmylib.so.2. A name without a directory is looked for
    where the system looks for libraries. resolve() loads the library if needed.

    A function can be resolved by name only if the library exports it with C linkage:

    \code
    extern "C" Q_DECL_EXPORT int average(int a, int b) { return (a + b) / 2; }
    \endcode

    Every QLibrary for the same file shares one loaded library, which stays in memory until each
    of them has called unload(), or the program ends. Destroying a QLibrary does not unload.

    \differences
    Libraries are loaded with \c dlopen(), so the class works on Linux, Android and macOS; on
    macOS the suffixes \c .dylib and \c .bundle are not added to a name. \c LoadArchiveMemberHint
    is ignored: it is for AIX. There is no QPluginLoader yet.
    \enddifferences

    \sa QObject
*/

/*!
    \enum QLibrary::LoadHint

    The hints change how load() loads the library.

    \value ResolveAllSymbolsHint Resolve every symbol of the library when it is loaded, and not when a function is first called. Loading fails at once if a symbol is missing.
    \value ExportExternalSymbolsHint Make the symbols of the library available to libraries that are loaded later.
    \value LoadArchiveMemberHint Allow the file name to name a member of an archive. Ignored.
    \value PreventUnloadHint Keep the library in memory when unload() is called, so that its static variables stay.
    \value DeepBindHint Let the library prefer its own symbols to those of the program and of libraries loaded before it. Only where the C library supports it.
*/

namespace {

struct Loaded {
    void *handle = nullptr;
    int references = 0;
};

std::mutex &registryMutex() {
    static std::mutex mutex;
    return mutex;
}
// The libraries that are loaded, by the path they were found at.
std::map<QString, Loaded> &registry() {
    static std::map<QString, Loaded> libraries;
    return libraries;
}

// "so" at the end, or before a version that is numbers and dots: libz.so, libz.so.1, libz.so.1.3.1.
bool hasLibrarySuffix(const QString &fileName) {
    const QStringList parts = fileName.mid(fileName.lastIndexOf(QChar(u'/')) + 1).split(QChar(u'.'));
    for (qsizetype i = parts.size() - 1; i >= 1; --i) {
        if (parts.at(i) == QString("so"))
            return true;
        bool number = false;
        parts.at(i).toInt(&number);
        if (!number)
            return false;
    }
    return false;
}

// The file names to try: as given, then with what the platform puts before and after the name of a library.
QStringList candidates(const QString &fileName, const QString &version) {
    const qsizetype slash = fileName.lastIndexOf(QChar(u'/'));
    const QString directory = fileName.left(slash + 1), name = fileName.mid(slash + 1);
    QStringList suffixes;
    if (hasLibrarySuffix(name)) {
        suffixes.append(QString());
    } else {
        if (!version.isEmpty())
            suffixes.append(QString(".so.") + version);
        suffixes.append(QString(".so"));
        suffixes.append(QString());
    }
    QStringList out;
    for (const QString &suffix : suffixes) {
        out.append(directory + name + suffix);
        if (!name.startsWith(QString("lib")))
            out.append(directory + QString("lib") + name + suffix);
    }
    return out;
}

} // namespace

/*!
    Constructs a library object with the given \a parent and no file name.

    \sa setFileName()
*/
QLibrary::QLibrary(QObject *parent) : QObject(parent) {}

/*!
    Constructs a library object with the given \a parent that will load the library \a fileName.
    Nothing is loaded yet.
*/
QLibrary::QLibrary(const QString &fileName, QObject *parent) : QObject(parent), m_fileName(fileName) {}

/*!
    Constructs a library object with the given \a parent that will load the major version
    \a verNum of the library \a fileName. A negative \a verNum means no version.
*/
QLibrary::QLibrary(const QString &fileName, int verNum, QObject *parent) : QObject(parent) {
    setFileNameAndVersion(fileName, verNum);
}

/*!
    Constructs a library object with the given \a parent that will load the version \a version,
    such as \c{1.2}, of the library \a fileName.
*/
QLibrary::QLibrary(const QString &fileName, const QString &version, QObject *parent)
    : QObject(parent), m_fileName(fileName), m_version(version) {}

/*!
    Destroys the library object. The library stays loaded: call unload() first to release it.
*/
QLibrary::~QLibrary() = default;

/*!
    Returns the address of the function \a symbol that the library exports, loading the library
    first if needed. Returns \c nullptr if the library cannot be loaded or has no such symbol;
    errorString() then says why. Cast the result to the type of the function.

    \sa load()
*/
QFunctionPointer QLibrary::resolve(const char *symbol) {
    if (!load())
        return nullptr;
#ifdef _WIN32
    Q_UNUSED(symbol)
    return nullptr;
#else
    void *handle = nullptr;
    {
        const std::lock_guard<std::mutex> lock(registryMutex());
        const auto it = registry().find(m_loadedPath);
        if (it != registry().end())
            handle = it->second.handle;
    }
    dlerror();
    void *address = handle ? dlsym(handle, symbol) : nullptr;
    if (!address) {
        const char *reason = dlerror();
        m_error = QString("Cannot resolve symbol \"") + QString::fromUtf8(symbol) + QString("\" in ") + m_loadedPath +
                  QString(": ") + (reason ? QString::fromLocal8Bit(reason) : QString("the symbol is not defined"));
        return nullptr;
    }
    m_error.clear();
    // POSIX requires that a pointer to an object converts to a pointer to a function, for this very use.
    return reinterpret_cast<QFunctionPointer>(address);
#endif
}

/*!
    \overload
    Loads the library \a fileName and returns the address of its function \a symbol, or
    \c nullptr. The library stays loaded.
*/
QFunctionPointer QLibrary::resolve(const QString &fileName, const char *symbol) {
    QLibrary library(fileName);
    return library.resolve(symbol);
}

/*!
    \overload
    Loads the major version \a verNum of the library \a fileName and returns the address of its
    function \a symbol, or \c nullptr.
*/
QFunctionPointer QLibrary::resolve(const QString &fileName, int verNum, const char *symbol) {
    QLibrary library(fileName, verNum);
    return library.resolve(symbol);
}

/*!
    \overload
    Loads the version \a version of the library \a fileName and returns the address of its
    function \a symbol, or \c nullptr.
*/
QFunctionPointer QLibrary::resolve(const QString &fileName, const QString &version, const char *symbol) {
    QLibrary library(fileName, version);
    return library.resolve(symbol);
}

/*!
    Loads the library and returns \c true if it is loaded, also when it was loaded before.
    If it cannot be loaded, returns \c false and errorString() says why. resolve() calls this, so
    a program rarely needs to.

    \sa unload(), isLoaded()
*/
bool QLibrary::load() {
    if (m_loaded)
        return true;
    if (m_fileName.isEmpty()) {
        m_error = QString("The shared library was not found.");
        return false;
    }
#ifdef _WIN32
    m_error = QString("Loading libraries is not supported on this platform.");
    return false;
#else
    int flags = m_hints.testFlag(ResolveAllSymbolsHint) ? RTLD_NOW : RTLD_LAZY;
    flags |= m_hints.testFlag(ExportExternalSymbolsHint) ? RTLD_GLOBAL : RTLD_LOCAL;
    if (m_hints.testFlag(PreventUnloadHint))
        flags |= RTLD_NODELETE;
#  ifdef RTLD_DEEPBIND
    if (m_hints.testFlag(DeepBindHint))
        flags |= RTLD_DEEPBIND;
#  endif
    QString reason;
    const QStringList names = candidates(m_fileName, m_version);
    for (const QString &name : names) {
        // A name with a directory must be that file; one without is left to the search of the system.
        if (name.contains(QChar(u'/')) && !QFileInfo::exists(name))
            continue;
        void *handle = dlopen(name.toLocal8Bit().constData(), flags);
        if (!handle) {
            if (reason.isEmpty()) {
                const char *text = dlerror();
                reason = text ? QString::fromLocal8Bit(text) : QString();
            }
            continue;
        }
        const QString path = name.contains(QChar(u'/')) ? QFileInfo(name).absoluteFilePath() : name;
        const std::lock_guard<std::mutex> lock(registryMutex());
        Loaded &entry = registry()[path];
        if (entry.references > 0)
            dlclose(handle); // the registry holds the one reference of the process
        else
            entry.handle = handle;
        ++entry.references;
        m_loadedPath = path;
        m_loaded = true;
        m_error.clear();
        return true;
    }
    m_error = QString("Cannot load library ") + m_fileName + QString(": ") +
              (reason.isEmpty() ? QString("the file was not found") : reason);
    return false;
#endif
}

/*!
    Releases the library. Returns \c true if that removed it from memory, and \c false if this
    object had not loaded it or another QLibrary still uses it.

    \sa load()
*/
bool QLibrary::unload() {
    if (!m_loaded)
        return false;
    m_loaded = false;
#ifdef _WIN32
    return false;
#else
    const std::lock_guard<std::mutex> lock(registryMutex());
    const auto it = registry().find(m_loadedPath);
    m_loadedPath.clear();
    if (it == registry().end() || --it->second.references > 0)
        return false;
    void *handle = it->second.handle;
    registry().erase(it);
    if (dlclose(handle) != 0) {
        const char *reason = dlerror();
        m_error = QString("Cannot unload library ") + m_fileName + QString(": ") +
                  (reason ? QString::fromLocal8Bit(reason) : QString());
        return false;
    }
    m_error.clear();
    return true;
#endif
}

/*!
    Returns \c true if load() succeeded on this object and unload() has not been called since.

    \sa load()
*/
bool QLibrary::isLoaded() const { return m_loaded; }

/*!
    Returns \c true if \a fileName has the suffix of a shared library: \c .so, also when a version
    follows it, as in \c libz.so.1.3.
*/
bool QLibrary::isLibrary(const QString &fileName) { return hasLibrarySuffix(fileName); }

/*!
    Sets the file name of the library to \a fileName, without a version. Leave out the prefix and
    suffix of the platform to stay portable. A QLibrary that is loaded then stands for the other
    library; the one that was loaded stays loaded.

    \sa fileName(), setFileNameAndVersion()
*/
void QLibrary::setFileName(const QString &fileName) {
    m_fileName = fileName;
    m_version.clear();
    m_loadedPath.clear();
    m_loaded = false;
}

/*!
    Returns the file name of the library: the name that was set or, once the library is loaded,
    the path of the file that was found.

    \sa setFileName()
*/
QString QLibrary::fileName() const { return m_loaded ? m_loadedPath : m_fileName; }

/*!
    Sets the file name to \a fileName and the version to load to the major version \a verNum.
    A negative \a verNum means no version.

    \sa setFileName()
*/
void QLibrary::setFileNameAndVersion(const QString &fileName, int verNum) {
    setFileName(fileName);
    m_version = verNum >= 0 ? QString::number(verNum) : QString();
}

/*!
    \overload
    Sets the file name to \a fileName and the version to load to \a version, such as \c{1.2}.
*/
void QLibrary::setFileNameAndVersion(const QString &fileName, const QString &version) {
    setFileName(fileName);
    m_version = version;
}

/*!
    Returns a text that says why the last call of load(), resolve() or unload() failed, or an
    empty string if it succeeded.
*/
QString QLibrary::errorString() const { return m_error; }

/*!
    Sets the hints that load() uses to \a hints. The default is no hint: symbols are resolved when
    they are first used and are not exported to other libraries. Hints that are set after the
    library is loaded have no effect.

    \sa loadHints()
*/
void QLibrary::setLoadHints(LoadHints hints) { m_hints = hints; }

/*!
    Returns the hints that load() uses.

    \sa setLoadHints()
*/
QLibrary::LoadHints QLibrary::loadHints() const { return m_hints; }

#include "moc_qlibrary.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
