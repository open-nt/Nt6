// MIT License - see LICENSE
//
// Tests for the QML engine of Nt Qml: documents, bindings and their dependencies, ids, declared properties,
// signals and functions, aliases, enumerations, C++ types, types written in QML, modules, components,
// Timer, Connections, singletons, contexts and errors.
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qfile.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qtimer.h>
#include <QtQml/QtQml>

#include "qml_classes.h"

#include <cstdio>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Makes the root object of `qml`; prints the errors if it cannot.
static QObject *make(QQmlEngine &engine, const char *qml, const QUrl &url = QUrl(QStringLiteral("file:///test.qml"))) {
    QQmlComponent component(&engine);
    component.setData(QByteArray(qml), url);
    QObject *o = component.create();
    if (!o)
        std::printf("  %s\n", component.errorString().toUtf8().constData());
    return o;
}

static QVariant prop(QObject *o, const char *name) { return o ? o->property(name) : QVariant(); }

static void wait(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void testBindings() {
    QQmlEngine engine;
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import QtQml 2.15
        QtObject {
            id: root
            property int width: 10
            property int height: width * 2
            property string text: "w=" + width
            property var list: [1, 2, 3]
            property real ratio: helper.factor * width
            property bool big: width > 50
            readonly property int twice: width * 2
            property QtObject helper: QtObject {
                id: helper
                property real factor: 0.5
            }
            function grow(by) { width += by; return width }
        }
    )QML"));
    QObject *o = root.get();
    CHECK(o);
    if (!o)
        return;
    CHECK(prop(o, "width").toInt() == 10);
    CHECK(prop(o, "height").toInt() == 20);
    CHECK(prop(o, "text").toString() == "w=10");
    CHECK(prop(o, "ratio").toDouble() == 5.0);
    CHECK(prop(o, "list").toList().size() == 3);
    CHECK(!prop(o, "big").toBool());

    // Changing a property updates the bindings that read it.
    QQmlEngine::contextForObject(o)->engine()->evaluate(QString()); // engine is alive
    QJSValue self = engine.newQObject(o);
    self.setProperty("width", 60);
    CHECK(prop(o, "height").toInt() == 120);
    CHECK(prop(o, "text").toString() == "w=60");
    CHECK(prop(o, "big").toBool());
    CHECK(prop(o, "twice").toInt() == 120);
    CHECK(self.property("grow").call({QJSValue(5)}).toInt() == 65);
    CHECK(prop(o, "height").toInt() == 130);

    // C++ reads and writes the declared properties too.
    o->setProperty("width", 1);
    CHECK(prop(o, "height").toInt() == 2);

    // An assignment replaces a binding.
    self.setProperty("height", 7);
    self.setProperty("width", 100);
    CHECK(prop(o, "height").toInt() == 7);
    // Qt.binding() makes a new one.
    engine.globalObject().setProperty("root", self);
    engine.evaluate("root.height = Qt.binding(function() { return root.width + 1 })");
    CHECK(prop(o, "height").toInt() == 101);
    self.setProperty("width", 3);
    CHECK(prop(o, "height").toInt() == 4);

    // A read-only property cannot be assigned.
    CHECK(engine.evaluate("try { root.twice = 3; 'no' } catch (e) { e.name }").toString() == "TypeError");
}

static void testSignalsAndHandlers() {
    QQmlEngine engine;
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import QtQml
        QtObject {
            id: root
            signal ping(int count, string who)
            property int pings: 0
            property string last
            property int changes: 0
            property int value: 1
            onPing: (count, who) => { root.pings += count; root.last = who }
            onValueChanged: changes++
            property var log: []
            Component.onCompleted: log.push("completed " + value)
            function fire() { ping(2, "fn") }
        }
    )QML"));
    QObject *o = root.get();
    CHECK(o);
    if (!o)
        return;
    QJSValue self = engine.newQObject(o);
    self.property("ping").call({3, QStringLiteral("cpp")});
    CHECK(prop(o, "pings").toInt() == 3 && prop(o, "last").toString() == "cpp");
    self.property("fire").call();
    CHECK(prop(o, "pings").toInt() == 5 && prop(o, "last").toString() == "fn");
    self.setProperty("value", 4);
    self.setProperty("value", 4);
    self.setProperty("value", 5);
    CHECK(prop(o, "changes").toInt() == 2);
    CHECK(self.property("log").property(0).toString() == "completed 1");
    // connect() from scripts.
    engine.globalObject().setProperty("root", self);
    engine.evaluate("var got = 0; root.ping.connect(function(n) { got += n }); root.ping(10, 'x')");
    CHECK(engine.evaluate("got").toInt() == 10);
    CHECK(prop(o, "pings").toInt() == 15);
}

static void testCppTypes() {
    qmlRegisterType<Gauge>("Test.Gauges", 1, 0, "Gauge");
    QQmlEngine engine;
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import Test.Gauges 1.0
        Gauge {
            id: main
            value: 21.5
            label: "temp " + value
            unit: "Fahrenheit"
            next: Gauge { id: second; value: main.value + 1 }
            property int alarms: 0
            property string reason
            onAlarm: function(level, reason) { main.alarms += level; main.reason = reason }
            property double viaMethod: doubled()
            property int unitValue: Gauge.Fahrenheit
            Gauge { objectName: "child"; value: 3 }
            Gauge { objectName: "child2" }
        }
    )QML"));
    auto *g = qobject_cast<Gauge *>(root.get());
    CHECK(g);
    if (!g)
        return;
    CHECK(g->value() == 21.5);
    CHECK(g->label() == "temp 21.5");
    CHECK(g->unit() == Gauge::Fahrenheit);
    CHECK(g->next() && g->next()->value() == 22.5);
    CHECK(g->m_parts.size() == 2 && g->m_parts.at(0)->objectName() == "child");
    CHECK(g->m_parts.at(0)->parent() == g);
    CHECK(g->begun == 1 && g->completedWithValue == 21.5);
    CHECK(prop(g, "viaMethod").toDouble() == 43.0);
    CHECK(prop(g, "unitValue").toInt() == Gauge::Fahrenheit);
    // C++ property changes update the bindings that read them.
    g->setValue(30);
    CHECK(g->label() == "temp 30");
    CHECK(g->next()->value() == 31);
    CHECK(prop(g, "viaMethod").toDouble() == 43.0); // method calls are not dependencies
    emit g->alarm(2, QStringLiteral("hot"));
    CHECK(prop(g, "alarms").toInt() == 2 && prop(g, "reason").toString() == "hot");
    CHECK(QQmlEngine::contextForObject(g) != nullptr);
    CHECK(qmlEngine(g) == &engine);
    CHECK(qmlContext(g)->objectForName("second") == g->next());
}

static void testDocumentsAndModules() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const auto write = [&](const QString &name, const char *text) {
        QDir().mkpath(QFileInfo(dir.filePath(name)).absolutePath());
        QFile f(dir.filePath(name));
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(text);
    };
    write("Badge.qml", R"QML(
        import QtQml
        QtObject {
            id: badge
            property string title: "badge"
            property int size: 2
            property alias caption: inner.text
            readonly property int area: size * size
            enum Kind { Small, Large = 5 }
            property QtObject inner: QtObject { id: inner; property string text: badge.title + "!" }
        }
    )QML");
    write("lib/Widgets/qmldir", "module Widgets\nPanel 1.0 Panel.qml\nsingleton Style 1.0 Style.qml\n");
    write("lib/Widgets/Panel.qml", "import QtQml\nQtObject { property string kind: \"panel \" + Style.accent }\n");
    write("lib/Widgets/Style.qml", "pragma Singleton\nimport QtQml\nQtObject { property string accent: \"blue\" }\n");
    write("main.qml", R"QML(
        import QtQml
        import Widgets 1.0
        import "." as Local
        QtObject {
            id: root
            property Badge badge: Badge { size: 3; title: "main" }
            property Local.Badge other: Local.Badge {}
            property Panel panel: Panel {}
            property int kind: Badge.Large
            component Pair: QtObject { property int a: 1; property int b: a + 1 }
            property Pair pair: Pair { a: 10 }
            property string caption: badge.caption
            property string accent: Style.accent
        }
    )QML");
    QQmlEngine engine;
    engine.addImportPath(dir.filePath("lib"));
    QQmlComponent component(&engine, QUrl::fromLocalFile(dir.filePath("main.qml")));
    CHECK(component.isReady());
    std::unique_ptr<QObject> root(component.create());
    if (!root)
        std::printf("  %s\n", component.errorString().toUtf8().constData());
    CHECK(root);
    if (!root)
        return;
    QObject *badge = prop(root.get(), "badge").value<QObject *>();
    CHECK(badge && prop(badge, "size").toInt() == 3 && prop(badge, "area").toInt() == 9);
    CHECK(prop(badge, "title").toString() == "main");
    CHECK(prop(badge, "caption").toString() == "main!");
    CHECK(prop(root.get(), "caption").toString() == "main!");
    QObject *other = prop(root.get(), "other").value<QObject *>();
    CHECK(other && prop(other, "title").toString() == "badge");
    QObject *panel = prop(root.get(), "panel").value<QObject *>();
    CHECK(panel && prop(panel, "kind").toString() == "panel blue");
    CHECK(prop(root.get(), "kind").toInt() == 5);
    QObject *pair = prop(root.get(), "pair").value<QObject *>();
    CHECK(pair && prop(pair, "b").toInt() == 11);
    CHECK(prop(root.get(), "accent").toString() == "blue");
    // Writing an alias writes its target, and the change reaches bindings on the alias.
    QJSValue b = engine.newQObject(badge);
    b.setProperty("caption", QStringLiteral("changed"));
    CHECK(prop(root.get(), "caption").toString() == "changed");

    // QQmlApplicationEngine loads a file and keeps its root object.
    QQmlApplicationEngine app;
    app.addImportPath(dir.filePath("lib"));
    QObject *created = nullptr;
    QObject::connect(&app, &QQmlApplicationEngine::objectCreated, [&](QObject *o, const QUrl &) { created = o; });
    app.load(dir.filePath("main.qml"));
    CHECK(app.rootObjects().size() == 1 && created == app.rootObjects().first());
}

static void testComponentsTimersConnections() {
    qmlRegisterType<Gauge>("Test.Gauges", 1, 0, "Gauge");
    QQmlEngine engine;
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import QtQml
        import Test.Gauges 1.0
        QtObject {
            id: root
            property int ticks: 0
            property int made: 0
            property var children: []
            property Component maker: Component {
                QtObject { property int n: root.made * 10 }
            }
            property Timer timer: Timer {
                interval: 5; repeat: true; running: true
                onTriggered: { root.ticks++; if (root.ticks >= 3) stop() }
            }
            property Gauge gauge: Gauge { id: gauge }
            property int heard: 0
            property Connections watch: Connections {
                target: gauge
                function onValueChanged() { root.heard++ }
            }
            function makeOne() { made++; var o = maker.createObject(root, { objectName: "made" + made }); children.push(o); return o.n }
        }
    )QML"));
    QObject *o = root.get();
    CHECK(o);
    if (!o)
        return;
    QJSValue self = engine.newQObject(o);
    CHECK(self.property("makeOne").call().toInt() == 10);
    CHECK(self.property("makeOne").call().toInt() == 20);
    CHECK(o->findChild<QObject *>("made2") != nullptr);
    for (int i = 0; i < 100 && prop(o, "ticks").toInt() < 3; ++i)
        wait(10);
    CHECK(prop(o, "ticks").toInt() == 3);
    wait(30);
    CHECK(prop(o, "ticks").toInt() == 3);
    auto *g = prop(o, "gauge").value<Gauge *>();
    CHECK(g);
    if (g) {
        g->setValue(1);
        g->setValue(2);
    }
    CHECK(prop(o, "heard").toInt() == 2);
}

// The enumerations of a C++ singleton are read through its name, as in Qt.
static void testSingletonEnums() {
    Gauge *instance = new Gauge;
    qmlRegisterSingletonInstance("Test.Singletons", 1, 0, "TheGauge", instance);
    QQmlEngine engine;
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import QtQml
        import Test.Singletons
        QtObject {
            property int unit: TheGauge.Fahrenheit
            property bool missing: TheGauge.NoSuchKey === undefined
        }
    )QML"));
    CHECK(root && prop(root.get(), "unit").toInt() == Gauge::Fahrenheit);
    CHECK(root && prop(root.get(), "missing").toBool());
    delete instance;
}

// A QObject without a parent that an invokable returns belongs to the engine, as in Qt.
static void testReturnedObjectOwnership() {
    qmlRegisterType<Spawner>("Test.Spawner", 1, 0, "Spawner");
    QObject *kept = nullptr;
    {
        QQmlEngine engine;
        std::unique_ptr<QObject> root(make(engine, R"QML(
            import QtQml
            import Test.Spawner
            QtObject {
                property Spawner spawner: Spawner {}
                property bool made: spawner.spawn() !== null && spawner.spawn() !== null
                property QtObject kept: spawner.spawnKept()
            }
        )QML"));
        CHECK(root && prop(root.get(), "made").toBool());
        CHECK(Spawner::alive == 3);
        kept = prop(root.get(), "kept").value<QObject *>();
    }
    CHECK(Spawner::alive == 1); // the engine deleted the two it owned
    delete kept;
    CHECK(Spawner::alive == 0);
}

static void testContextsAndErrors() {
    QQmlEngine engine;
    engine.setOutputWarningsToStandardError(false);
    Settings settings;
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("appName", QStringLiteral("Demo"));
    std::unique_ptr<QObject> root(make(engine, R"QML(
        import QtQml
        QtObject {
            property string title: appName + " (" + settings.theme + ")"
            property int count: 0
        }
    )QML"));
    CHECK(root && prop(root.get(), "title").toString() == "Demo (light)");
    settings.setTheme(QStringLiteral("dark"));
    CHECK(prop(root.get(), "title").toString() == "Demo (dark)");
    engine.rootContext()->setContextProperty("appName", QStringLiteral("Other"));
    CHECK(prop(root.get(), "title").toString() == "Other (dark)");

    // Initial properties win over the bindings of the document.
    QQmlComponent component(&engine);
    component.setData("import QtQml\nQtObject { property int count: 5; property int twice: count * 2 }",
                      QUrl(QStringLiteral("file:///init.qml")));
    std::unique_ptr<QObject> init(component.createWithInitialProperties({{"count", 21}}));
    CHECK(init && prop(init.get(), "count").toInt() == 21 && prop(init.get(), "twice").toInt() == 42);

    // Errors.
    QQmlComponent bad(&engine);
    bad.setData("import QtQml\nQtObject {\n  property int x: \n}", QUrl(QStringLiteral("file:///bad.qml")));
    CHECK(bad.isError() && bad.errors().size() == 1 && bad.errors().first().line() == 4);
    QQmlComponent unknown(&engine);
    unknown.setData("import QtQml\nQtObject { Rectangle {} }", QUrl(QStringLiteral("file:///unknown.qml")));
    CHECK(unknown.isReady() && !unknown.create());
    CHECK(unknown.errorString().contains("Rectangle is not a type"));
    QQmlComponent noModule(&engine);
    noModule.setData("import NoSuchModule 1.0\nQtObject {}", QUrl(QStringLiteral("file:///nomodule.qml")));
    CHECK(!noModule.create() && noModule.errorString().contains("is not installed"));
    QQmlComponent noProperty(&engine);
    noProperty.setData("import QtQml\nQtObject { nonsense: 3 }", QUrl(QStringLiteral("file:///noprop.qml")));
    std::unique_ptr<QObject> np(noProperty.create());
    CHECK(noProperty.errorString().contains("non-existent property"));

    // A binding that throws is a warning, not a crash.
    QList<QQmlError> warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &w) { warnings += w; });
    std::unique_ptr<QObject> throwing(make(engine, "import QtQml\nQtObject { property int x: undefinedName.y }"));
    CHECK(throwing && !warnings.isEmpty());
    CHECK(!warnings.isEmpty() && warnings.first().description().contains("undefinedName"));
    // A binding loop is detected.
    warnings.clear();
    std::unique_ptr<QObject> loop(
        make(engine, "import QtQml\nQtObject { property int a: b + 1; property int b: a + 1 }"));
    CHECK(loop);
    bool sawLoop = false;
    for (const QQmlError &w : warnings)
        sawLoop = sawLoop || w.description().contains("binding loop");
    CHECK(sawLoop);
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testBindings();
    testSignalsAndHandlers();
    testCppTypes();
    testDocumentsAndModules();
    testComponentsTimersConnections();
    testSingletonEnums();
    testReturnedObjectOwnership();
    testContextsAndErrors();
    if (g_failures)
        std::printf("%d failure(s)\n", g_failures);
    else
        std::printf("all QML tests passed\n");
    return g_failures ? 1 : 0;
}
