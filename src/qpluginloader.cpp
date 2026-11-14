// MIT License - see LICENSE
#include <QtCore/qpluginloader.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qpointer.h>

#include <map>
#include <mutex>
#include <vector>

namespace {

// The plugins that QPluginLoader objects have loaded, by the path of the library.
struct LoadedPlugin {
    int loaders = 0;
    QPointer<QObject> instance;
};

std::mutex &pluginMutex() {
    static auto *m = new std::mutex;
    return *m;
}

std::map<QString, LoadedPlugin> &loadedPlugins() {
    static auto *plugins = new std::map<QString, LoadedPlugin>;
    return *plugins;
}

std::vector<QStaticPlugin> &staticPluginList() {
    static auto *list = new std::vector<QStaticPlugin>;
    return *list;
}

QJsonObject parseMetaData(const QPluginMetaData &raw) {
    if (!raw.data || raw.size == 0)
        return QJsonObject();
    const QByteArray bytes(reinterpret_cast<const char *>(raw.data), static_cast<qsizetype>(raw.size));
    return QJsonDocument::fromJson(bytes).object();
}

// The library file that `name` stands for: the file itself if it exists, or else one in a directory of
// QCoreApplication::libraryPaths(), as a library name without "lib" and ".so" too. Empty if there is none.
QString locate(const QString &name) {
    if (name.isEmpty())
        return QString();
    if (QFileInfo::exists(name))
        return QFileInfo(name).canonicalFilePath();
    if (QFileInfo(name).isAbsolute())
        return QString();
    QStringList names{name};
    if (!QLibrary::isLibrary(name)) {
        names.append(name + QStringLiteral(".so"));
        if (!name.startsWith(QStringLiteral("lib")))
            names.append(QStringLiteral("lib") + name + QStringLiteral(".so"));
    }
    for (const QString &dir : QCoreApplication::libraryPaths()) {
        for (const QString &candidate : names) {
            const QString path = QDir(dir).filePath(candidate);
            if (QFileInfo::exists(path))
                return QFileInfo(path).canonicalFilePath();
        }
    }
    return QString();
}

} // namespace

namespace QtPrivate {

QByteArray pluginMetaData(const char *iid, const char *className, const char *json) {
    QJsonObject object;
    object.insert(QStringLiteral("IID"), QString::fromUtf8(iid));
    object.insert(QStringLiteral("className"), QString::fromUtf8(className));
    if (json && *json) {
        QJsonParseError error{};
        const QJsonDocument file = QJsonDocument::fromJson(QByteArray(json), &error);
        if (error.error != QJsonParseError::NoError || !file.isObject())
            qWarning("Plugin %s: the metadata file is not a JSON object", className);
        else
            object.insert(QStringLiteral("MetaData"), file.object());
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace QtPrivate

/*!
    Returns the metadata of the plugin: an object with the keys \c IID, \c className and, if
    \c Q_PLUGIN_METADATA named a file, \c MetaData.

    \sa QPluginLoader::metaData()
*/
QJsonObject QStaticPlugin::metaData() const { return parseMetaData(rawMetaDataFn()); }

/*!
    \class QStaticPlugin
    \since 5.2
    \brief The QStaticPlugin class is a struct containing a reference to a static plugin instance together with
    its meta data.

    \c Q_IMPORT_PLUGIN registers one for each static plugin that the application links; QPluginLoader::staticPlugins()
    lists them.

    \differences
    The metadata is JSON text, not CBOR.
    \enddifferences

    \sa QPluginLoader
*/

// Registers a static plugin. \c Q_IMPORT_PLUGIN calls it.
void qRegisterStaticPluginFunction(QStaticPlugin plugin) {
    const std::lock_guard<std::mutex> lock(pluginMutex());
    staticPluginList().push_back(plugin);
}

/*!
    \class QPluginLoader
    \brief The QPluginLoader class loads a plugin at run-time.

    A plugin is a shared library whose root class has \c Q_PLUGIN_METADATA. Load it, then cast its root object to
    the interface that the application expects:

    \code
    QPluginLoader loader(QStringLiteral("echoplugin"));
    if (auto *echo = qobject_cast<EchoInterface *>(loader.instance()))
        qDebug() << echo->echo(QStringLiteral("hello"));
    else
        qWarning() << loader.errorString();
    \endcode

    A file name without a directory is looked for in QCoreApplication::libraryPaths(), with or without the \c lib
    prefix and the \c .so suffix. All loaders of one plugin share its root object, which is deleted when the last
    of them unloads the plugin; a loader that is destroyed keeps the plugin loaded, as in Qt.

    \section1 Writing a plugin

    The root class of a plugin derives from QObject and from the interface, and has \c Q_PLUGIN_METADATA with
    \c IID followed by the identifier of the interface, as a string or a macro that expands to one, and optionally
    \c FILE followed by the name of a JSON file, relative to the header, whose object becomes the \c MetaData of
    the plugin. ntmoc reads it and writes the functions that QPluginLoader looks up:

    \code
    class EchoPlugin : public QObject, public EchoInterface {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.example.EchoInterface" FILE "echoplugin.json")
    public:
        QString echo(const QString &message) override { return message; }
    };
    \endcode

    Build it as a shared module, such as with \c{add_library(echoplugin MODULE ...)}, with ntmoc as \c AUTOMOC.
    When Nt is built as static libraries, which is the default, the plugin must not link Nt itself: give it Nt's
    include directories only, and build the application with its symbols exported (the CMake property
    \c ENABLE_EXPORTS), so that the plugin uses the application's copy of Nt. A second copy in the plugin would
    initialize the application's meta-objects again when it is loaded. With Nt as shared libraries, link the
    plugin to them as usual.

    \section1 Static plugins

    A plugin built into a static library with \c QT_STATICPLUGIN defined is a static plugin. The application
    links it and registers it with \c Q_IMPORT_PLUGIN, given the name of the root class, in one of its source
    files, outside any function; staticInstances() and staticPlugins() then return it:

    \code
    Q_IMPORT_PLUGIN(EchoPlugin)
    \endcode

    \differences
    The metadata is JSON text in the library rather than CBOR in a section of its own, so metaData() loads the
    library (without making the root object) to read it, and Qt's plugins cannot be loaded. Plugins are not
    checked for the Nt version or build key that they were built with. Only POSIX shared libraries (\c dlopen())
    are supported. With Nt as static libraries, a plugin uses the application's copy of Nt, as described above.
    \enddifferences

    \sa QLibrary, QStaticPlugin
*/

/*!
    Constructs a plugin loader with the given \a parent.

    \sa setFileName()
*/
QPluginLoader::QPluginLoader(QObject *parent) : QObject(parent) {}

/*!
    Constructs a plugin loader with the given \a parent that will load the plugin \a fileName.

    \sa setFileName()
*/
QPluginLoader::QPluginLoader(const QString &fileName, QObject *parent) : QObject(parent) { setFileName(fileName); }

/*!
    Destroys the loader. The plugin stays loaded; call unload() first to release it.
*/
QPluginLoader::~QPluginLoader() = default;

bool QPluginLoader::loadPlugin() {
    if (m_loaded)
        return true;
    if (m_fileName.isEmpty()) {
        m_error = QStringLiteral("The shared library was not found.");
        return false;
    }
    m_library.setFileName(m_fileName);
    if (!m_library.load()) {
        m_error = m_library.errorString();
        return false;
    }
    if (!m_library.resolve("qt_plugin_query_metadata_v2") || !m_library.resolve("qt_plugin_instance")) {
        m_error = QStringLiteral("The file '") + m_fileName + QStringLiteral("' is not a valid Qt plugin.");
        m_library.unload();
        return false;
    }
    {
        const std::lock_guard<std::mutex> lock(pluginMutex());
        ++loadedPlugins()[m_fileName].loaders;
    }
    m_loaded = true;
    m_error.clear();
    return true;
}

/*!
    Returns the root object of the plugin, loading the plugin first if needed, or \c nullptr if it cannot be
    loaded; errorString() then says why. The object is shared by all loaders of the plugin and deleted when the
    plugin is unloaded.

    \sa load(), unload()
*/
QObject *QPluginLoader::instance() {
    if (!loadPlugin())
        return nullptr;
    auto make = reinterpret_cast<QtPluginInstanceFunction>(m_library.resolve("qt_plugin_instance"));
    if (!make)
        return nullptr;
    QObject *object = make();
    const std::lock_guard<std::mutex> lock(pluginMutex());
    loadedPlugins()[m_fileName].instance = object;
    return object;
}

/*!
    Returns the metadata of the plugin: an object with the keys \c IID and \c className and, if
    \c Q_PLUGIN_METADATA named a file, \c MetaData with that file's object. Returns an empty object if the file is
    not a plugin.

    \sa instance()
*/
QJsonObject QPluginLoader::metaData() const {
    if (m_haveMetaData)
        return m_metaData;
    QLibrary probe(m_fileName);
    probe.setLoadHints(m_library.loadHints());
    auto query = reinterpret_cast<QtPluginMetaDataFunction>(probe.resolve("qt_plugin_query_metadata_v2"));
    if (query) {
        m_metaData = parseMetaData(query());
        m_haveMetaData = true;
    }
    probe.unload();
    return m_metaData;
}

/*!
    Returns the root objects of the static plugins that \c Q_IMPORT_PLUGIN registered, making them if needed.

    \sa staticPlugins()
*/
QObjectList QPluginLoader::staticInstances() {
    QObjectList out;
    for (const QStaticPlugin &plugin : staticPlugins())
        out.append(plugin.instance());
    return out;
}

/*!
    \since 5.2
    Returns the static plugins that \c Q_IMPORT_PLUGIN registered, with their metadata.

    \sa staticInstances()
*/
QList<QStaticPlugin> QPluginLoader::staticPlugins() {
    const std::lock_guard<std::mutex> lock(pluginMutex());
    QList<QStaticPlugin> out;
    for (const QStaticPlugin &plugin : staticPluginList())
        out.append(plugin);
    return out;
}

/*!
    Loads the plugin and returns \c true if it was loaded or is a plugin that is loaded already; otherwise returns
    \c false and errorString() says why. instance() loads the plugin itself.

    \sa unload(), isLoaded()
*/
bool QPluginLoader::load() { return loadPlugin(); }

/*!
    Unloads the plugin and returns \c true if that removed it from memory. The root object is deleted when the
    last loader unloads the plugin. Returns \c false if this loader had not loaded the plugin, or if another
    loader or QLibrary still uses it.

    \sa load(), instance()
*/
bool QPluginLoader::unload() {
    if (!m_loaded) {
        m_error = QStringLiteral("The plugin was not loaded.");
        return false;
    }
    m_loaded = false;
    QPointer<QObject> doomed;
    {
        const std::lock_guard<std::mutex> lock(pluginMutex());
        const auto it = loadedPlugins().find(m_fileName);
        if (it != loadedPlugins().end() && --it->second.loaders == 0) {
            doomed = it->second.instance;
            loadedPlugins().erase(it);
        }
    }
    delete doomed.data();
    const bool removed = m_library.unload();
    if (!removed && !m_library.errorString().isEmpty())
        m_error = m_library.errorString();
    return removed;
}

/*!
    Returns \c true if this loader has loaded the plugin; otherwise returns \c false.

    \sa load()
*/
bool QPluginLoader::isLoaded() const { return m_loaded; }

/*!
    Sets the file of the plugin to \a fileName. A name that is not a path to a file is looked for in
    QCoreApplication::libraryPaths(), with or without the \c lib prefix and the \c .so suffix. A loaded plugin
    is not unloaded.

    \sa fileName()
*/
void QPluginLoader::setFileName(const QString &fileName) {
    if (m_loaded)
        unload();
    const QString found = locate(fileName);
    m_fileName = found.isEmpty() ? fileName : found;
    m_haveMetaData = false;
    m_metaData = QJsonObject();
    if (found.isEmpty() && !fileName.isEmpty())
        m_error = QStringLiteral("The shared library was not found.");
}

/*!
    Returns the file of the plugin: the path that setFileName() found, or the name as given if it found none.

    \sa setFileName()
*/
QString QPluginLoader::fileName() const { return m_fileName; }

/*!
    Returns a text that describes the last error, or \c{"Unknown error"} if there was none.
*/
QString QPluginLoader::errorString() const { return m_error.isEmpty() ? QStringLiteral("Unknown error") : m_error; }

/*!
    \since 4.4
    Sets the hints that load() passes to \c dlopen() to \a loadHints, as QLibrary::setLoadHints() does.

    \sa loadHints()
*/
void QPluginLoader::setLoadHints(QLibrary::LoadHints loadHints) { m_library.setLoadHints(loadHints); }

/*!
    \since 4.4
    Returns the load hints.

    \sa setLoadHints()
*/
QLibrary::LoadHints QPluginLoader::loadHints() const { return m_library.loadHints(); }

#include "moc_qpluginloader.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
