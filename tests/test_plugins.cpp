// MIT License - see LICENSE
// QPluginLoader, Q_PLUGIN_METADATA, Q_IMPORT_PLUGIN and QStaticPlugin.
#include "plugins/echointerface.h"

#include <QCoreApplication>
#include <QFactoryInterface>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPluginLoader>
#include <QPointer>
#include <QtPlugin>

#include <cstdio>

Q_IMPORT_PLUGIN(StaticEchoPlugin)

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                       \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static const QString kPluginDir = QStringLiteral(NT_TEST_PLUGIN_DIR);
static const QString kPlugin = kPluginDir + QStringLiteral("/libnt_test_echoplugin.so");

static void testStaticPlugins() {
    const QList<QStaticPlugin> plugins = QPluginLoader::staticPlugins();
    CHECK(plugins.size() == 1);
    if (plugins.isEmpty())
        return;
    const QJsonObject meta = plugins[0].metaData();
    CHECK(meta.value(QStringLiteral("IID")).toString() == QStringLiteral("org.nt.test.EchoInterface"));
    CHECK(meta.value(QStringLiteral("className")).toString() == QStringLiteral("StaticEchoPlugin"));
    CHECK(!meta.contains(QStringLiteral("MetaData")));
    CHECK(plugins[0].rawMetaData() != nullptr && plugins[0].rawMetaDataSize() > 0);

    const QObjectList instances = QPluginLoader::staticInstances();
    CHECK(instances.size() == 1);
    CHECK(instances.value(0) == plugins[0].instance());
    auto *echo = qobject_cast<EchoInterface *>(instances.value(0));
    CHECK(echo && echo->echo(QStringLiteral("x")) == QStringLiteral("static:x"));
    auto *factory = qobject_cast<QFactoryInterface *>(instances.value(0));
    CHECK(factory && factory->keys() == QStringList{QStringLiteral("static")});
}

static void testLoader() {
    QPluginLoader loader(kPlugin);
    CHECK(loader.fileName() == QFileInfo(kPlugin).canonicalFilePath());
    CHECK(!loader.isLoaded());

    // The metadata can be read before the plugin is loaded.
    const QJsonObject meta = loader.metaData();
    CHECK(meta.value(QStringLiteral("IID")).toString() == QStringLiteral(EchoInterface_iid));
    CHECK(meta.value(QStringLiteral("className")).toString() == QStringLiteral("EchoPlugin"));
    const QJsonObject custom = meta.value(QStringLiteral("MetaData")).toObject();
    CHECK(custom.value(QStringLiteral("Version")).toInt() == 2);
    CHECK(custom.value(QStringLiteral("Keys")).toArray().size() == 2);
    CHECK(!loader.isLoaded());

    QObject *root = loader.instance();
    CHECK(root != nullptr);
    CHECK(loader.isLoaded());
    auto *echo = qobject_cast<EchoInterface *>(root);
    CHECK(echo && echo->echo(QStringLiteral("hi")) == QStringLiteral("echo:hi"));
    CHECK(root && QString::fromLatin1(root->metaObject()->className()) == QStringLiteral("EchoPlugin"));
    CHECK(loader.instance() == root);

    // The loaders of one plugin share its root object, which goes with the last unload().
    QPluginLoader second(kPlugin);
    CHECK(second.instance() == root);
    QPointer<QObject> guard(root);
    CHECK(!second.unload());
    CHECK(!guard.isNull());
    CHECK(!second.isLoaded() && loader.isLoaded());
    CHECK(loader.unload());
    CHECK(guard.isNull());
    CHECK(!loader.isLoaded());
    CHECK(!loader.unload());
    CHECK(loader.errorString() == QStringLiteral("The plugin was not loaded."));

    // Loading again makes a new root object.
    CHECK(loader.load());
    QObject *again = loader.instance();
    CHECK(again != nullptr && qobject_cast<EchoInterface *>(again));
    CHECK(loader.unload());
}

static void testLibraryPaths() {
    QCoreApplication::addLibraryPath(kPluginDir);
    QPluginLoader byName(QStringLiteral("nt_test_echoplugin"));
    CHECK(byName.fileName() == QFileInfo(kPlugin).canonicalFilePath());
    CHECK(byName.load());
    CHECK(byName.unload());
    QCoreApplication::removeLibraryPath(kPluginDir);

    QPluginLoader property;
    property.setProperty("fileName", kPlugin);
    CHECK(property.fileName() == QFileInfo(kPlugin).canonicalFilePath());
    property.setLoadHints(QLibrary::ResolveAllSymbolsHint);
    CHECK(property.loadHints() == QLibrary::ResolveAllSymbolsHint);
}

static void testErrors() {
    QPluginLoader missing(QStringLiteral("/nonexistent/libnothing.so"));
    CHECK(!missing.load());
    CHECK(missing.instance() == nullptr);
    CHECK(!missing.errorString().isEmpty() && missing.errorString() != QStringLiteral("Unknown error"));
    CHECK(missing.metaData().isEmpty());

    // A shared library that is not a plugin.
    QPluginLoader notPlugin(kPluginDir + QStringLiteral("/libnt_test_plugin.so"));
    CHECK(!notPlugin.load());
    CHECK(notPlugin.errorString().contains(QStringLiteral("not a valid Qt plugin")));
    CHECK(notPlugin.metaData().isEmpty());
    CHECK(!notPlugin.isLoaded());

    QPluginLoader empty;
    CHECK(empty.errorString() == QStringLiteral("Unknown error"));
    CHECK(!empty.load());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testStaticPlugins();
    testLoader();
    testLibraryPaths();
    testErrors();
    if (g_failures == 0)
        std::printf("All plugin tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
