// MIT License - see LICENSE
#include <QtQml/qqmlapplicationengine.h>
#include <QtQml/qqmlcomponent.h>

#include "qjsengine_p.h"
#include "qqmlengine_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

using QtQmlPrivate::EngineData;

/*!
    \class QQmlComponent
    \brief The QQmlComponent class makes objects from a QML document.

    A component reads a document once and makes objects from it as often as needed:

    \snippet qml/hello_qml.cpp component

    create() makes the objects of the document in a new context, sets their bindings and completes them.
    beginCreate() and completeCreate() split this in two, to set properties in between. The caller owns
    the object.

    A \c Component declared inside a document is a QQmlComponent too; scripts make its object with
    createObject().

    \differences
    Loading is always synchronous, and only local files and resources can be loaded: there are no network
    URLs, no incubators and no \c createObject() with an incubation mode.
    \enddifferences

    \sa QQmlEngine, QQmlContext
*/

/*!
    \enum QQmlComponent::CompilationMode

    \value PreferSynchronous Load at once.
    \value Asynchronous Load at once in Nt too.
*/

/*!
    \enum QQmlComponent::Status

    \value Null No document is loaded.
    \value Ready The document is loaded and can make objects.
    \value Loading Never: Nt loads at once.
    \value Error The document could not be loaded or has errors; see errors().
*/

/*!
    Constructs a component without an engine or a document, with the given \a parent.
*/
QQmlComponent::QQmlComponent(QObject *parent) : QObject(parent) {}

/*!
    Constructs a component of \a engine without a document, with the given \a parent.

    \sa setData(), loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, QObject *parent) : QObject(parent), m_engine(engine) {}

/*!
    Constructs a component of \a engine with the given \a parent that loads the file \a fileName.
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QString &fileName, QObject *parent)
    : QQmlComponent(engine, fileName, PreferSynchronous, parent) {}

/*!
    Constructs a component of \a engine with the given \a parent that loads the file \a fileName; \a mode
    is not used.
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QString &fileName, CompilationMode mode, QObject *parent)
    : QQmlComponent(engine, parent) {
    loadUrl(fileName.startsWith(QLatin1String(":/")) ? QUrl(QStringLiteral("qrc") + fileName)
                                                     : QUrl::fromLocalFile(QFileInfo(fileName).absoluteFilePath()),
            mode);
}

/*!
    Constructs a component of \a engine with the given \a parent that loads the document at \a url.
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QUrl &url, QObject *parent)
    : QQmlComponent(engine, url, PreferSynchronous, parent) {}

/*!
    Constructs a component of \a engine with the given \a parent that loads the document at \a url;
    \a mode is not used.
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QUrl &url, CompilationMode mode, QObject *parent)
    : QQmlComponent(engine, parent) {
    loadUrl(url, mode);
}

/*!
    Destroys the component. The objects it made are not destroyed.
*/
QQmlComponent::~QQmlComponent() = default;

void QQmlComponent::setStatus(Status status) {
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged(status);
    emit progressChanged(progress());
}

/*!
    Returns the status of the component.
*/
QQmlComponent::Status QQmlComponent::status() const { return m_status; }

/*!
    Returns \c true if no document is loaded; otherwise returns \c false.
*/
bool QQmlComponent::isNull() const { return m_status == Null; }

/*!
    Returns \c true if the component can make objects; otherwise returns \c false.
*/
bool QQmlComponent::isReady() const { return m_status == Ready; }

/*!
    Returns \c true if the document could not be loaded or has errors; otherwise returns \c false.
*/
bool QQmlComponent::isError() const { return m_status == Error; }

/*!
    Returns \c false: Nt loads documents at once.
*/
bool QQmlComponent::isLoading() const { return m_status == Loading; }

/*!
    Returns the errors of loading the document and of the last creation.
*/
QList<QQmlError> QQmlComponent::errors() const { return m_errors; }

/*!
    Returns the errors as text, one per line.
*/
QString QQmlComponent::errorString() const {
    QStringList lines;
    for (const QQmlError &e : m_errors)
        lines.append(e.toString());
    return lines.join(u'\n');
}

/*!
    Returns 1.0 if the component is ready or failed, and 0.0 otherwise.
*/
qreal QQmlComponent::progress() const { return m_status == Ready || m_status == Error ? 1.0 : 0.0; }

/*!
    Returns the URL of the document.
*/
QUrl QQmlComponent::url() const { return m_url; }

/*!
    Loads the document at \a url, resolved against the base URL of the engine.
*/
void QQmlComponent::loadUrl(const QUrl &url) { loadUrl(url, PreferSynchronous); }

/*!
    \overload

    Loads the document at \a url; \a mode is not used.
*/
void QQmlComponent::loadUrl(const QUrl &url, CompilationMode mode) {
    (void)mode;
    m_errors.clear();
    m_root = nullptr;
    m_program.reset();
    EngineData *d = EngineData::of(m_engine);
    if (!d) {
        QQmlError e;
        e.setDescription(QStringLiteral("the component has no engine"));
        m_errors.append(e);
        setStatus(Error);
        return;
    }
    m_url = d->resolve(m_engine->baseUrl(), url);
    m_program = d->load(m_url, &m_errors);
    setStatus(m_program ? Ready : Error);
}

/*!
    Loads the type \a typeName of the module \a uri; \a mode is not used. A type written in QML is loaded
    from its document; a C++ type is made directly.
*/
void QQmlComponent::loadFromModule(const QString &uri, const QString &typeName, CompilationMode mode) {
    const QtQmlPrivate::TypeInfo *t = QtQmlPrivate::TypeRegistry::instance().find(uri, typeName);
    if (t && !t->url.isEmpty()) {
        loadUrl(t->url, mode);
        return;
    }
    m_errors.clear();
    m_program.reset();
    if (!t || !t->create) {
        QQmlError e;
        e.setDescription(t ? QStringLiteral("%1 cannot be made").arg(typeName)
                           : QStringLiteral("module \"%1\" has no type \"%2\"").arg(uri, typeName));
        m_errors.append(e);
        setStatus(Error);
        return;
    }
    // A document with one object of the type.
    setData(QStringLiteral("import %1\n%2 {}\n").arg(uri, typeName).toUtf8(),
            QUrl(QStringLiteral("module:%1/%2.qml").arg(uri, typeName)));
}

/*!
    Reads the document \a data, whose relative URLs are resolved against \a baseUrl.
*/
void QQmlComponent::setData(const QByteArray &data, const QUrl &baseUrl) {
    m_errors.clear();
    m_root = nullptr;
    EngineData *d = EngineData::of(m_engine);
    if (!d) {
        QQmlError e;
        e.setDescription(QStringLiteral("the component has no engine"));
        m_errors.append(e);
        setStatus(Error);
        return;
    }
    m_url = baseUrl.isEmpty() ? QUrl() : d->resolve(m_engine->baseUrl(), baseUrl);
    m_program = d->parse(data, m_url, &m_errors);
    setStatus(m_program ? Ready : Error);
}

/*!
    Makes the objects of the document in a new context whose parent is \a context, or the creation
    context of the component, or the root context of the engine. Returns the root object, which the
    caller owns, or \c nullptr on error; see errors().
*/
QObject *QQmlComponent::create(QQmlContext *context) {
    QObject *o = beginCreate(context);
    completeCreate();
    return o;
}

/*!
    Makes the objects as create() does, with \a initialProperties set on the root object before it is
    completed. \a context is the parent context.
*/
QObject *QQmlComponent::createWithInitialProperties(const QVariantMap &initialProperties, QQmlContext *context) {
    m_initialProperties = initialProperties;
    QObject *o = create(context);
    m_initialProperties.clear();
    return o;
}

/*!
    Sets the \a properties of \a component, an object made by beginCreate() and not yet completed.
*/
void QQmlComponent::setInitialProperties(QObject *component, const QVariantMap &properties) {
    EngineData *d = EngineData::of(m_engine);
    if (!d || !component)
        return;
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        QString error;
        d->removeBinding(component, it.key());
        if (!d->writeProperty(component, it.key(), d->rt->fromVariant(it.value()), &error)) {
            QQmlError e;
            e.setUrl(m_url);
            e.setDescription(error);
            m_errors.append(e);
        }
    }
}

/*!
    Makes the objects of the document in a new context whose parent is \a context, without setting their
    bindings or completing them; completeCreate() does that. Returns the root object.
*/
QObject *QQmlComponent::beginCreate(QQmlContext *context) {
    if (m_status != Ready || !m_program) {
        if (m_errors.isEmpty()) {
            QQmlError e;
            e.setUrl(m_url);
            e.setDescription(QStringLiteral("the component is not ready"));
            m_errors.append(e);
        }
        return nullptr;
    }
    // Errors of a previous creation are forgotten.
    QList<QQmlError> kept;
    m_errors = kept;
    QQmlContext *parentContext = context             ? context
                                 : m_creationContext ? m_creationContext.data()
                                                     : m_engine->rootContext();
    // A Component declared in a document makes its object in the context it was declared in; a document
    // has a context of its own.
    auto *c = new QQmlContext(parentContext);
    if (!m_url.isEmpty())
        c->setBaseUrl(m_url);
    m_pending = std::make_unique<QtQmlPrivate::Creator>(m_engine, QtQmlPrivate::Document{m_program, m_url});
    QObject *o = m_pending->create(m_root, c, m_initialProperties);
    m_errors.append(m_pending->errors);
    if (!o || !m_pending->errors.isEmpty()) {
        m_pending->errors.clear();
        delete o;
        delete c;
        m_pending.reset();
        return nullptr;
    }
    c->setParent(o);
    return o;
}

/*!
    Sets the bindings of the objects that beginCreate() made, and completes them.
*/
void QQmlComponent::completeCreate() {
    if (!m_pending)
        return;
    m_pending->complete();
    m_errors.append(m_pending->errors);
    m_pending.reset();
    if (EngineData *d = EngineData::of(m_engine))
        for (const QQmlError &e : std::as_const(m_errors))
            d->warn(e);
}

/*!
    Returns the context that a Component declared in a document was made in, or \c nullptr.
*/
QQmlContext *QQmlComponent::creationContext() const { return m_creationContext.data(); }

/*!
    Returns the engine of the component.
*/
QQmlEngine *QQmlComponent::engine() const { return m_engine; }

/*!
    Makes the object of the component, as scripts do with \c{component.createObject(parent, properties)}:
    with the QObject \a parent and \a properties set before it is completed. An object made by a script
    without a parent belongs to the script engine, which deletes it when no script can reach it.
*/
QObject *QQmlComponent::createObject(QObject *parent, const QVariantMap &properties) {
    m_initialProperties = properties;
    QObject *o = beginCreate(m_creationContext ? m_creationContext.data() : nullptr);
    m_initialProperties.clear();
    if (o && parent) {
        o->setParent(parent);
        if (EngineData *d = EngineData::of(m_engine); d && d->setVisualParent)
            d->setVisualParent(o, parent);
    }
    completeCreate();
    if (o && !parent)
        if (QtJSPrivate::Runtime *rt = QtJSPrivate::EngineAccess::runtime(m_engine))
            rt->scriptOwned.insert(o, true);
    return o;
}

// ================================================================================================================

/*!
    \class QQmlApplicationEngine
    \brief The QQmlApplicationEngine class loads the main QML document of an application.

    The engine loads a document, makes its root object, and keeps it until the engine is destroyed:

    \snippet qml/hello_qml.cpp application

    \c{Qt.quit()} and \c{Qt.exit()} in documents quit the application.

    \differences
    There are no file selectors (setExtraFileSelectors() keeps them), no translations and no network URLs.
    \enddifferences

    \sa QQmlEngine, QQmlComponent
*/

/*!
    Constructs an engine with the given \a parent.
*/
QQmlApplicationEngine::QQmlApplicationEngine(QObject *parent) : QQmlEngine(parent) {
    connect(this, &QQmlEngine::quit, QCoreApplication::instance(), [] { QCoreApplication::quit(); });
    connect(this, &QQmlEngine::exit, QCoreApplication::instance(), [](int code) { QCoreApplication::exit(code); });
}

/*!
    Constructs an engine with the given \a parent and loads the document at \a url.
*/
QQmlApplicationEngine::QQmlApplicationEngine(const QUrl &url, QObject *parent) : QQmlApplicationEngine(parent) {
    load(url);
}

/*!
    Constructs an engine with the given \a parent and loads the file \a filePath.
*/
QQmlApplicationEngine::QQmlApplicationEngine(const QString &filePath, QObject *parent) : QQmlApplicationEngine(parent) {
    load(filePath);
}

/*!
    Constructs an engine with the given \a parent and loads the type \a typeName of the module \a uri.
*/
QQmlApplicationEngine::QQmlApplicationEngine(const QString &uri, const QString &typeName, QObject *parent)
    : QQmlApplicationEngine(parent) {
    loadFromModule(uri, typeName);
}

/*!
    Destroys the engine and the root objects it made.
*/
QQmlApplicationEngine::~QQmlApplicationEngine() {
    for (const QPointer<QObject> &o : std::as_const(m_roots))
        delete o.data();
}

/*!
    Returns the root objects of the documents loaded.
*/
QList<QObject *> QQmlApplicationEngine::rootObjects() const {
    QList<QObject *> list;
    for (const QPointer<QObject> &o : m_roots)
        if (o)
            list.append(o.data());
    return list;
}

/*!
    Sets the properties that the root objects of documents loaded later get to \a initialProperties.
*/
void QQmlApplicationEngine::setInitialProperties(const QVariantMap &initialProperties) {
    m_initialProperties = initialProperties;
}

void QQmlApplicationEngine::loadComponent(QQmlComponent *component, const QUrl &url) {
    if (!component->isReady()) {
        for (const QQmlError &e : component->errors())
            qWarning().noquote() << e.toString();
        emit objectCreated(nullptr, url);
        emit objectCreationFailed(url);
        return;
    }
    QObject *o = component->createWithInitialProperties(m_initialProperties);
    if (!o) {
        emit objectCreated(nullptr, url);
        emit objectCreationFailed(url);
        return;
    }
    m_roots.append(o);
    emit objectCreated(o, url);
}

/*!
    Loads the document at \a url and makes its root object.
*/
void QQmlApplicationEngine::load(const QUrl &url) {
    QQmlComponent component(this, url);
    loadComponent(&component, component.url());
}

/*!
    \overload

    Loads the file \a filePath; a path starting with \c{:/} is a resource.
*/
void QQmlApplicationEngine::load(const QString &filePath) {
    load(filePath.startsWith(QLatin1String(":/")) ? QUrl(QStringLiteral("qrc") + filePath)
                                                  : QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()));
}

/*!
    Loads the type \a typeName of the module \a uri and makes an object of it.
*/
void QQmlApplicationEngine::loadFromModule(const QString &uri, const QString &typeName) {
    QQmlComponent component(this);
    component.loadFromModule(uri, typeName);
    loadComponent(&component, component.url());
}

/*!
    Keeps \a extraFileSelectors; Nt has no file selectors.
*/
void QQmlApplicationEngine::setExtraFileSelectors(const QStringList &extraFileSelectors) {
    m_selectors = extraFileSelectors;
}

/*!
    Reads the document \a data, whose relative URLs are resolved against \a url, and makes its root object.
*/
void QQmlApplicationEngine::loadData(const QByteArray &data, const QUrl &url) {
    QQmlComponent component(this);
    component.setData(data, url);
    loadComponent(&component, url);
}

#include "moc_qqmlapplicationengine.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qqmlcomponent.cpp"         // NOLINT(bugprone-suspicious-include): generated by ntmoc
