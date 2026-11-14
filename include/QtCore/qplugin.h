// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qpointer.h>

#include <cstddef>

class QObject;

// The type of the function that returns the root object of a plugin.
using QtPluginInstanceFunction = QObject *(*)();

/*!
    \class QPluginMetaData
    \since 6.0
    \brief The QPluginMetaData struct holds the metadata that a plugin carries.

    \c Q_PLUGIN_METADATA in the root class of a plugin makes it; QPluginLoader::metaData() and QStaticPlugin::metaData() read it.

    \differences
    The data is JSON text in Nt, not CBOR, so it cannot be read by Qt, nor Qt's by Nt.
    \enddifferences
*/
struct QPluginMetaData {
    /*!
        \variable QPluginMetaData::data
        The bytes of the metadata.
    */
    const uchar *data;
    /*!
        \variable QPluginMetaData::size
        The number of bytes at \c data.
    */
    size_t size;
};

// The type of the function that returns the metadata of a plugin.
using QtPluginMetaDataFunction = QPluginMetaData (*)();

class QStaticPlugin {
public:
    /*!
        Constructs a static plugin from the function \a i that returns its root object and the function \a m
        that returns its metadata. \c Q_PLUGIN_METADATA makes these for a plugin that is built with
        \c QT_STATICPLUGIN defined.
    */
    constexpr QStaticPlugin(QtPluginInstanceFunction i, QtPluginMetaDataFunction m) : instance(i), rawMetaDataFn(m) {}

    /*!
        \variable QStaticPlugin::instance
        The function that returns the root object of the plugin, made on the first call.
    */
    QtPluginInstanceFunction instance;

    /*!
        Returns the metadata of the plugin as raw bytes.

        \sa rawMetaDataSize(), metaData()
    */
    const void *rawMetaData() const { return rawMetaDataFn().data; }
    /*!
        Returns the number of bytes of the metadata.

        \sa rawMetaData()
    */
    qsizetype rawMetaDataSize() const { return static_cast<qsizetype>(rawMetaDataFn().size); }
    QJsonObject metaData() const;

private:
    QtPluginMetaDataFunction rawMetaDataFn;
};

// Registers a static plugin; Q_IMPORT_PLUGIN() calls it.
void qRegisterStaticPluginFunction(QStaticPlugin plugin);

namespace QtPrivate {
// The metadata of a plugin: its interface identifier, its class name, and the JSON object from the file that
// Q_PLUGIN_METADATA() names (or an empty string). Called by the code that ntmoc generates.
QByteArray pluginMetaData(const char *iid, const char *className, const char *json);
} // namespace QtPrivate

// The root object of a plugin, made on the first call and remade if it was deleted.
#define Q_PLUGIN_INSTANCE(IMPLEMENTATION)                                                                              \
    {                                                                                                                  \
        static QPointer<QObject> _instance;                                                                            \
        if (!_instance)                                                                                                \
            _instance = new IMPLEMENTATION;                                                                            \
        return _instance;                                                                                              \
    }

#define NT_PLUGIN_METADATA_BODY(IID, PLUGINCLASSNAME, JSON)                                                            \
    {                                                                                                                  \
        static const QByteArray _data = QtPrivate::pluginMetaData(IID, #PLUGINCLASSNAME, JSON);                        \
        return QPluginMetaData{reinterpret_cast<const uchar *>(_data.constData()), static_cast<size_t>(_data.size())}; \
    }

// What ntmoc writes for a class with Q_PLUGIN_METADATA(): the functions that QPluginLoader looks up in a shared
// library, or, with QT_STATICPLUGIN defined, the function that Q_IMPORT_PLUGIN() registers.
#if defined(QT_STATICPLUGIN)
#define NT_MOC_EXPORT_PLUGIN(PLUGINCLASS, PLUGINCLASSNAME, IID, JSON)                                                  \
    static QObject *qt_plugin_instance_##PLUGINCLASSNAME()                                                             \
        Q_PLUGIN_INSTANCE(PLUGINCLASS) static QPluginMetaData qt_plugin_query_metadata_##PLUGINCLASSNAME()             \
            NT_PLUGIN_METADATA_BODY(IID, PLUGINCLASSNAME, JSON) QStaticPlugin qt_static_plugin_##PLUGINCLASSNAME();    \
    QStaticPlugin qt_static_plugin_##PLUGINCLASSNAME() {                                                               \
        return QStaticPlugin(qt_plugin_instance_##PLUGINCLASSNAME, qt_plugin_query_metadata_##PLUGINCLASSNAME);        \
    }
#else
#define NT_MOC_EXPORT_PLUGIN(PLUGINCLASS, PLUGINCLASSNAME, IID, JSON)                                                  \
    extern "C" Q_DECL_EXPORT QObject *qt_plugin_instance();                                                            \
    extern "C" Q_DECL_EXPORT QObject *qt_plugin_instance()                                                             \
        Q_PLUGIN_INSTANCE(PLUGINCLASS) extern "C" Q_DECL_EXPORT QPluginMetaData qt_plugin_query_metadata_v2();         \
    extern "C" Q_DECL_EXPORT QPluginMetaData qt_plugin_query_metadata_v2()                                             \
        NT_PLUGIN_METADATA_BODY(IID, PLUGINCLASSNAME, JSON)
#endif

// Registers the static plugin whose root class is PLUGIN, so that QPluginLoader::staticPlugins() lists it. See the
// documentation of QPluginLoader.
#define Q_IMPORT_PLUGIN(PLUGIN)                                                                                        \
    QStaticPlugin qt_static_plugin_##PLUGIN();                                                                         \
    namespace {                                                                                                        \
    struct Static##PLUGIN##PluginInstance {                                                                            \
        Static##PLUGIN##PluginInstance() { qRegisterStaticPluginFunction(qt_static_plugin_##PLUGIN()); }               \
    };                                                                                                                 \
    const Static##PLUGIN##PluginInstance static##PLUGIN##Instance;                                                     \
    }
