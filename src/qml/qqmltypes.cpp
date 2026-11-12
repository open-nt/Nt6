// MIT License - see LICENSE
//
// The registry of QML types, the functions of <qqml.h> that fill it, and the C++ types of the QtQml module.
#include <QtQml/qqml.h>

#include "qqmltypes_p.h"

#include <QtCore/qmutex.h>

namespace QtQmlPrivate {

namespace {

QMutex g_registryMutex;

} // namespace

TypeRegistry &TypeRegistry::instance() {
    static TypeRegistry *registry = [] {
        auto *r = new TypeRegistry;
        // The QtQml module.
        TypeInfo object;
        object.module = QStringLiteral("QtQml");
        object.major = 2;
        object.name = QStringLiteral("QtObject");
        object.metaObject = &QObject::staticMetaObject;
        object.create = [] { return new QObject; };
        r->add(object);
        TypeInfo timer = object;
        timer.name = QStringLiteral("Timer");
        timer.metaObject = &QQmlTimer::staticMetaObject;
        timer.create = [] { return static_cast<QObject *>(new QQmlTimer); };
        r->add(timer);
        TypeInfo connections = object;
        connections.name = QStringLiteral("Connections");
        connections.metaObject = &QQmlConnections::staticMetaObject;
        connections.create = [] { return static_cast<QObject *>(new QQmlConnections); };
        r->add(connections);
        TypeInfo component = object;
        component.name = QStringLiteral("Component");
        component.metaObject = &QQmlComponent::staticMetaObject;
        component.create = nullptr;
        component.noCreationReason = QStringLiteral("Component is made from its declaration");
        r->add(component);
        return r;
    }();
    return *registry;
}

void TypeRegistry::add(const TypeInfo &type) {
    QMutexLocker lock(&g_registryMutex);
    for (TypeInfo &t : types) {
        if (t.module == type.module && t.name == type.name && !t.name.isEmpty()) {
            t = type;
            return;
        }
    }
    types.append(type);
    if (!modules.contains(type.module))
        modules.append(type.module);
}

const TypeInfo *TypeRegistry::find(const QString &module, const QString &name) const {
    QMutexLocker lock(&g_registryMutex);
    for (const TypeInfo &t : types)
        if (t.name == name && (module.isEmpty() || t.module == module))
            return &t;
    return nullptr;
}

bool TypeRegistry::hasModule(const QString &module) const {
    QMutexLocker lock(&g_registryMutex);
    return modules.contains(module);
}

int registerType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QMetaObject *mo,
                 std::function<QObject *()> create, const QString &noCreationReason) {
    TypeInfo t;
    t.module = QString::fromUtf8(uri);
    t.major = versionMajor;
    t.minor = versionMinor;
    t.name = qmlName ? QString::fromUtf8(qmlName) : QString();
    t.metaObject = mo;
    t.create = std::move(create);
    t.noCreationReason = noCreationReason;
    TypeRegistry &r = TypeRegistry::instance();
    r.add(t);
    return static_cast<int>(r.types.size()) - 1;
}

int registerSingleton(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QMetaObject *mo,
                      std::function<QObject *(QQmlEngine *, QJSEngine *)> create, QObject *instance) {
    TypeInfo t;
    t.module = QString::fromUtf8(uri);
    t.major = versionMajor;
    t.minor = versionMinor;
    t.name = QString::fromUtf8(qmlName);
    t.metaObject = mo;
    t.singleton = true;
    t.singletonFactory = std::move(create);
    t.singletonInstance = instance;
    TypeRegistry &r = TypeRegistry::instance();
    r.add(t);
    return static_cast<int>(r.types.size()) - 1;
}

} // namespace QtQmlPrivate

// A C++ class becomes a QML type with qmlRegisterType(), in a module that documents import:
// import Thermostat 1.0
// Thermometer { celsius: 21 }
// Its properties, signals, slots and Q_INVOKABLE methods are those of its meta-object. A class whose
// Q_CLASSINFO("DefaultProperty", "name") names a property gets the objects declared inside it in that
// property; a class that inherits QQmlParserStatus is told when its component is complete.

// Registers the C++ type T as the QML type qmlName of the module uri, version versionMajor.
// versionMinor. T must be a QObject with Q_OBJECT and a default constructor. Returns the
// number of the type.
// Versions are kept but any version of a module imports all its types.

// Registers T for the module uri, version versionMajor, without a name: documents use objects
// of the type, from properties, but cannot make them.

// Registers T as qmlName of the module uri, version versionMajor.versionMinor, as a type
// that documents cannot make; trying to gives reason as the error.

// Registers a singleton of type T as typeName of the module uri, version versionMajor.
// versionMinor. callback makes the instance when a document first uses it; each engine has its
// own, and owns it if it has no parent.

// Registers cppObject as the singleton typeName of the module uri, version versionMajor.
// versionMinor. All engines share it; the caller keeps ownership.
int qmlRegisterSingletonInstance(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                 QObject *cppObject) {
    return QtQmlPrivate::registerSingleton(uri, versionMajor, versionMinor, typeName,
                                           cppObject ? cppObject->metaObject() : nullptr, nullptr, cppObject);
}

// Registers the QML document at url as the type qmlName of the module uri, version
// versionMajor.versionMinor.
int qmlRegisterType(const QUrl &url, const char *uri, int versionMajor, int versionMinor, const char *qmlName) {
    QtQmlPrivate::TypeInfo t;
    t.module = QString::fromUtf8(uri);
    t.major = versionMajor;
    t.minor = versionMinor;
    t.name = QString::fromUtf8(qmlName);
    t.url = url;
    QtQmlPrivate::TypeRegistry &r = QtQmlPrivate::TypeRegistry::instance();
    r.add(t);
    return static_cast<int>(r.types.size()) - 1;
}

// Registers the QML document at url as the singleton qmlName of the module uri, version
// versionMajor.versionMinor. Each engine makes one instance, when a document first uses it.
int qmlRegisterSingletonType(const QUrl &url, const char *uri, int versionMajor, int versionMinor,
                             const char *qmlName) {
    QtQmlPrivate::TypeInfo t;
    t.module = QString::fromUtf8(uri);
    t.major = versionMajor;
    t.minor = versionMinor;
    t.name = QString::fromUtf8(qmlName);
    t.url = url;
    t.singleton = true;
    QtQmlPrivate::TypeRegistry &r = QtQmlPrivate::TypeRegistry::instance();
    r.add(t);
    return static_cast<int>(r.types.size()) - 1;
}

// Makes the module uri, version versionMajor.versionMinor, importable even if it has no types
// yet.
void qmlRegisterModule(const char *uri, int versionMajor, int versionMinor) {
    (void)versionMajor;
    (void)versionMinor;
    QtQmlPrivate::TypeRegistry &r = QtQmlPrivate::TypeRegistry::instance();
    const QString module = QString::fromUtf8(uri);
    QMutexLocker lock(&QtQmlPrivate::g_registryMutex);
    if (!r.modules.contains(module))
        r.modules.append(module);
}

// ---- Timer ----

QQmlTimer::QQmlTimer(QObject *parent) : QObject(parent) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] { fire(); });
}

void QQmlTimer::setInterval(int interval) {
    if (interval == m_interval)
        return;
    m_interval = interval;
    update();
    emit intervalChanged();
}

void QQmlTimer::setRunning(bool running) {
    if (running == m_running)
        return;
    m_running = running;
    update();
    emit runningChanged();
    if (running && m_triggeredOnStart && m_complete)
        emit triggered();
}

void QQmlTimer::setRepeating(bool repeat) {
    if (repeat == m_repeat)
        return;
    m_repeat = repeat;
    update();
    emit repeatChanged();
}

void QQmlTimer::setTriggeredOnStart(bool on) {
    if (on == m_triggeredOnStart)
        return;
    m_triggeredOnStart = on;
    emit triggeredOnStartChanged();
}

void QQmlTimer::classBegin() { m_complete = false; }

void QQmlTimer::componentComplete() {
    m_complete = true;
    update();
    if (m_running && m_triggeredOnStart)
        emit triggered();
}

void QQmlTimer::start() { setRunning(true); }

void QQmlTimer::stop() { setRunning(false); }

void QQmlTimer::restart() {
    m_timer.stop();
    setRunning(true);
    update();
}

void QQmlTimer::update() {
    if (!m_complete)
        return;
    if (!m_running) {
        m_timer.stop();
        return;
    }
    m_timer.setSingleShot(!m_repeat);
    m_timer.start(std::max(0, m_interval));
}

void QQmlTimer::fire() {
    if (!m_repeat) {
        m_running = false;
        emit runningChanged();
    }
    emit triggered();
}

// ---- Connections ----

QQmlConnections::QQmlConnections(QObject *parent) : QObject(parent) {}

void QQmlConnections::setTarget(QObject *target) {
    m_targetSet = true;
    if (target == m_target)
        return;
    disconnectAll();
    m_target = target;
    if (m_complete)
        connectAll();
    emit targetChanged();
}

void QQmlConnections::setEnabled(bool enabled) {
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void QQmlConnections::classBegin() { m_complete = false; }

void QQmlConnections::componentComplete() {
    m_complete = true;
    // Without a target, the parent is the target.
    if (!m_targetSet)
        m_target = parent();
    connectAll();
}

void QQmlConnections::disconnectAll() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    if (m_target && !m_declaredConnections.empty()) {
        if (QtQmlPrivate::EngineData *d = QtQmlPrivate::EngineData::of(this))
            if (QtQmlPrivate::ObjectData *od = d->objects.value(m_target.data()))
                for (const QString &signal : m_declaredConnections) {
                    auto &list = od->handlers[signal];
                    for (auto it = list.begin(); it != list.end();)
                        it = it->thisObject.isObject() && d->rt->unwrapQObject(it->thisObject) == this ? list.erase(it)
                                                                                                       : it + 1;
                }
    }
    m_declaredConnections.clear();
}

void QQmlConnections::connectAll() {
    QObject *target = m_target.data();
    QtQmlPrivate::EngineData *d = QtQmlPrivate::EngineData::of(this);
    if (!target || !d)
        return;
    const QMetaObject *mo = target->metaObject();
    for (const auto &entry : handlers) {
        const QString &signal = entry.first;
        QtQmlPrivate::Handler h = entry.second;
        QtQmlPrivate::ObjectData *od = d->objects.value(target);
        if (od && od->signalParameters.contains(signal)) {
            // A signal declared in QML; the Connections object is the handler's "this", to find it again.
            h.thisObject = d->rt->wrapQObject(this);
            od->handlers[signal].push_back(h);
            m_declaredConnections.push_back(signal);
            continue;
        }
        const QByteArray name = signal.toUtf8();
        QMetaMethod method;
        for (int i = mo->methodCount() - 1; i >= 0; --i) {
            const QMetaMethod m = mo->method(i);
            if (m.methodType() == QMetaMethod::Signal && m.name() == name &&
                (!method.isValid() || m.parameterCount() > method.parameterCount()))
                method = m;
        }
        if (!method.isValid()) {
            if (!m_ignoreUnknown)
                d->warn(
                    location,
                    QStringLiteral("Connections: no signal \"%1\" in %2").arg(signal, QtQmlPrivate::typeName(target)));
            continue;
        }
        QStringList names;
        for (const QByteArray &n : method.parameterNames())
            names.append(QString::fromUtf8(n));
        QPointer<QQmlConnections> self = this;
        m_connections.push_back(
            QtPrivate::Access::connectFunction(target, method, this, [self, d, h, method, names](void *const *argv) {
                if (!self || !self->m_enabled)
                    return;
                QtJSPrivate::Values args;
                for (int i = 0; i < method.parameterCount(); ++i)
                    args.push_back(d->rt->fromVariant(QVariant(method.parameterMetaType(i), argv[i])));
                d->callHandler(h, args, names);
            }));
    }
}

#include "moc_qqmltypes_p.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
