// MIT License - see LICENSE
//
// Making objects from QML documents. Types are found through the imports of the document: C++ types
// registered in modules, QML files in the document's directory and in imported directories, modules in
// the import paths described by qmldir files, and inline components.
//
// An object is made, then its declared properties, signals and functions are added, then its children
// are made. Bindings and values are set when the whole tree exists, so that ids can be used anywhere;
// then the objects are completed.
#include "qqmlengine_p.h"
#include "qqmltypes_p.h"

#include "qjsbuiltins_p.h"
#include "qjsinterp_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qmetaobject.h>

#include <cmath>

using QtJSPrivate::Node;
using QtJSPrivate::QmlBinding;
using QtJSPrivate::Throw;

namespace QtQmlPrivate {

namespace {

bool urlExists(const QUrl &url) {
    if (url.scheme() == QLatin1String("qrc"))
        return QFile::exists(u':' + url.path());
    if (url.isLocalFile())
        return QFileInfo::exists(url.toLocalFile());
    if (url.isRelative())
        return QFileInfo::exists(url.path());
    return false;
}

QByteArray readUrl(const QUrl &url) {
    QString path;
    if (url.scheme() == QLatin1String("qrc"))
        path = u':' + url.path();
    else if (url.isLocalFile())
        path = url.toLocalFile();
    else
        path = url.path();
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QUrl directoryOf(const QUrl &url) { return url.resolved(QUrl(QStringLiteral("./"))); }

QUrl subdirectory(const QUrl &dir, const QString &relative) {
    QString r = relative;
    if (!r.endsWith(u'/'))
        r += u'/';
    return dir.resolved(QUrl(r));
}

// A constant written as a binding: it is assigned once instead of being a binding.
bool constantValue(const Node *n, Value *out) {
    if (!n)
        return false;
    switch (n->kind) {
    case Node::Kind::Number:
        *out = Value::number(n->num);
        return true;
    case Node::Kind::String:
        *out = Value::string(n->str);
        return true;
    case Node::Kind::True:
        *out = Value::boolean(true);
        return true;
    case Node::Kind::False:
        *out = Value::boolean(false);
        return true;
    case Node::Kind::Null:
        *out = Value::null();
        return true;
    case Node::Kind::Template:
        if (!n->list.empty())
            return false;
        *out = Value::string(n->strs.value(0));
        return true;
    case Node::Kind::Unary:
        if ((n->op == QtJSPrivate::Op::Minus || n->op == QtJSPrivate::Op::Plus) && n->a->kind == Node::Kind::Number) {
            *out = Value::number(n->op == QtJSPrivate::Op::Minus ? -n->a->num : n->a->num);
            return true;
        }
        return false;
    default:
        return false;
    }
}

Value defaultValue(Runtime &rt, const QString &type) {
    if (type == QLatin1String("int") || type == QLatin1String("real") || type == QLatin1String("double") ||
        type == QLatin1String("number"))
        return Value::number(0);
    if (type == QLatin1String("bool"))
        return Value::boolean(false);
    if (type == QLatin1String("string") || type == QLatin1String("url"))
        return Value::string(QString());
    if (type.startsWith(QLatin1String("list<")))
        return Value::object(rt.newArray());
    if (type == QLatin1String("var") || type == QLatin1String("variant") || type == QLatin1String("date"))
        return Value();
    if (type == QLatin1String("color"))
        return Value::string(QStringLiteral("#000000"));
    if (type == QLatin1String("point"))
        return Value::object(rt.newObject());
    if (!type.isEmpty() && type.at(0).isUpper())
        return Value::null();
    return Value();
}

QString lowerFirst(const QString &s) { return s.isEmpty() ? s : s.at(0).toLower() + s.mid(1); }

bool isHandlerName(const QString &name) {
    return name.size() > 2 && name.startsWith(QLatin1String("on")) && name.at(2).isUpper();
}

} // namespace

// The imports of a document, and the types they bring.
struct Imports {
    struct Entry {
        QString uri;    // a module
        QUrl directory; // or a directory of QML files
        QString alias;
        QHash<QString, std::pair<QUrl, bool>> qmldir; // type name: file, singleton
    };
    struct Type {
        const TypeInfo *cpp = nullptr;
        QUrl url;
        bool singleton = false;
        const QmlObjectNode *inlineNode = nullptr;
        bool isComponent = false;
        bool found() const { return cpp || !url.isEmpty() || inlineNode || isComponent; }
    };

    Imports(EngineData *d, const Document &document) : data(d), doc(document) {}

    void prepare(QList<QQmlError> *errors);
    Type resolve(const QString &name) const;
    Type resolveIn(const Entry &e, const QString &name) const;

    EngineData *data;
    Document doc;
    std::vector<Entry> entries;
    QHash<QString, std::pair<QUrl, bool>> localQmldir; // the qmldir of the document's own directory
    QHash<QString, const QmlObjectNode *> inlineComponents;
    bool prepared = false;
};

namespace {

QHash<QString, std::pair<QUrl, bool>> readQmldir(const QUrl &dir) {
    QHash<QString, std::pair<QUrl, bool>> types;
    const QString text = QString::fromUtf8(readUrl(dir.resolved(QUrl(QStringLiteral("qmldir")))));
    for (const QString &raw : text.split(u'\n')) {
        const QString line = raw.section(u'#', 0, 0).simplified();
        const QStringList t = line.split(u' ', Qt::SkipEmptyParts);
        if (t.size() >= 4 && t.at(0) == QLatin1String("singleton") && t.at(3).endsWith(QLatin1String(".qml")))
            types.insert(t.at(1), {dir.resolved(QUrl(t.at(3))), true});
        else if (t.size() >= 3 && t.at(0) == QLatin1String("internal") && t.at(2).endsWith(QLatin1String(".qml")))
            types.insert(t.at(1), {dir.resolved(QUrl(t.at(2))), false});
        else if (t.size() >= 3 && t.at(2).endsWith(QLatin1String(".qml")))
            types.insert(t.at(0), {dir.resolved(QUrl(t.at(2))), false});
    }
    return types;
}

void collectInline(const QmlObjectNode *node, QHash<QString, const QmlObjectNode *> *out) {
    if (!node)
        return;
    for (const auto &c : node->components) {
        out->insert(c.first, c.second);
        collectInline(c.second, out);
    }
    for (const QmlObjectNode *child : node->children)
        collectInline(child, out);
    for (const QmlBinding *b : node->bindings) {
        collectInline(b->object, out);
        for (const QmlObjectNode *o : b->objects)
            collectInline(o, out);
    }
}

} // namespace

void Imports::prepare(QList<QQmlError> *errors) {
    if (prepared)
        return;
    prepared = true;
    collectInline(doc.program->qml.root, &inlineComponents);
    const QUrl here = directoryOf(doc.url);
    if (!doc.url.isEmpty())
        localQmldir = readQmldir(here);
    for (const QtJSPrivate::QmlImport &imp : doc.program->qml.imports) {
        Entry e;
        e.alias = imp.alias;
        if (!imp.path.isEmpty()) {
            const QUrl target = here.resolved(QUrl(imp.path));
            if (imp.path.endsWith(QLatin1String(".js"))) {
                QQmlError err;
                err.setUrl(doc.url);
                err.setLine(imp.line);
                err.setDescription(QStringLiteral("JavaScript imports are not supported: \"%1\"").arg(imp.path));
                errors->append(err);
                continue;
            }
            e.directory = subdirectory(here, imp.path);
            e.qmldir = readQmldir(e.directory);
            entries.push_back(e);
            continue;
        }
        e.uri = imp.uri;
        bool found = TypeRegistry::instance().hasModule(imp.uri) || imp.uri == QLatin1String("QtQml");
        // A module of QML files in the import paths, described by its qmldir file.
        QStringList paths = data->importPaths;
        paths << QStringLiteral("qrc:/qt/qml") << QStringLiteral("qrc:/") << here.toString();
        for (const QString &p : std::as_const(paths)) {
            const QUrl base = p.startsWith(QLatin1String("qrc:")) || p.contains(QLatin1String("://"))
                                  ? QUrl(p.endsWith(u'/') ? p : p + u'/')
                                  : QUrl::fromLocalFile(QDir(p).absolutePath() + u'/');
            const QUrl dir = subdirectory(base, imp.uri.split(u'.').join(u'/'));
            if (urlExists(dir.resolved(QUrl(QStringLiteral("qmldir"))))) {
                e.directory = dir;
                e.qmldir = readQmldir(dir);
                found = true;
                break;
            }
        }
        if (!found) {
            QQmlError err;
            err.setUrl(doc.url);
            err.setLine(imp.line);
            err.setDescription(QStringLiteral("module \"%1\" is not installed").arg(imp.uri));
            errors->append(err);
            continue;
        }
        entries.push_back(e);
    }
}

Imports::Type Imports::resolveIn(const Entry &e, const QString &name) const {
    Type t;
    if (!e.uri.isEmpty()) {
        if (const TypeInfo *info = TypeRegistry::instance().find(e.uri, name)) {
            t.cpp = info->url.isEmpty() ? info : nullptr;
            t.url = info->url;
            t.singleton = info->singleton;
            if (!t.cpp && t.url.isEmpty())
                t.cpp = info;
            return t;
        }
    }
    if (e.qmldir.contains(name)) {
        t.url = e.qmldir.value(name).first;
        t.singleton = e.qmldir.value(name).second;
        return t;
    }
    if (!e.directory.isEmpty() && e.uri.isEmpty() && !name.isEmpty() && name.at(0).isUpper()) {
        const QUrl file = e.directory.resolved(QUrl(name + QStringLiteral(".qml")));
        if (urlExists(file))
            t.url = file;
    }
    return t;
}

Imports::Type Imports::resolve(const QString &name) const {
    Type t;
    if (name == QLatin1String("Component") || name == QLatin1String("QtQml.Component")) {
        t.isComponent = true;
        return t;
    }
    const qsizetype dot = name.indexOf(u'.');
    if (dot > 0) {
        const QString qualifier = name.left(dot), rest = name.mid(dot + 1);
        for (const Entry &e : entries)
            if (e.alias == qualifier || (e.alias.isEmpty() && e.uri == qualifier)) {
                t = resolveIn(e, rest);
                if (t.found())
                    return t;
            }
        if (const QmlObjectNode *n = inlineComponents.value(rest); n && dot > 0) {
            t.inlineNode = n;
            return t;
        }
        return t;
    }
    if (const QmlObjectNode *n = inlineComponents.value(name)) {
        t.inlineNode = n;
        return t;
    }
    // The directory of the document comes first, then the imports.
    if (localQmldir.contains(name) && localQmldir.value(name).first != doc.url) {
        t.url = localQmldir.value(name).first;
        t.singleton = localQmldir.value(name).second;
        return t;
    }
    if (!name.isEmpty() && name.at(0).isUpper()) {
        const QUrl file = directoryOf(doc.url).resolved(QUrl(name + QStringLiteral(".qml")));
        if (!doc.url.isEmpty() && urlExists(file) && file != doc.url) {
            t.url = file;
            return t;
        }
    }
    for (const Entry &e : entries) {
        if (!e.alias.isEmpty())
            continue;
        t = resolveIn(e, name);
        if (t.found())
            return t;
    }
    // QtQml's own types are always available.
    if (const TypeInfo *info = TypeRegistry::instance().find(QStringLiteral("QtQml"), name)) {
        t.cpp = info;
        return t;
    }
    return t;
}

namespace {

// Names of types used as values in expressions: singletons, and enumerations by type ("Timer.Something").
class TypeScope : public Object {
public:
    TypeScope(std::shared_ptr<Imports> i, QQmlEngine *e, EngineData *d, const QString &q = QString())
        : Object(nullptr, Class::Host), imports(std::move(i)), engine(e), data(d), qualifier(q) {}

    bool lookup(Runtime &rt, const QString &name, Value *out) {
        if (name.isEmpty() || !name.at(0).isUpper())
            return false;
        if (cache.contains(name)) {
            *out = cache.value(name);
            return true;
        }
        if (misses.contains(name))
            return false;
        // An import alias is a namespace of the types it imports.
        if (qualifier.isEmpty())
            for (const Imports::Entry &e : imports->entries)
                if (e.alias == name) {
                    auto *ns = rt.heap.make<TypeScope>(imports, engine, data, name);
                    cache.insert(name, Value::object(ns));
                    *out = Value::object(ns);
                    return true;
                }
        const Imports::Type t = imports->resolve(qualifier.isEmpty() ? name : qualifier + u'.' + name);
        Value v;
        bool found = false;
        if (t.cpp && t.singleton) {
            QObject *instance = data->singleton(t.cpp->module, t.cpp->name);
            if (!instance)
                instance = t.cpp->singletonInstance.data();
            v = rt.wrapQObject(instance);
            found = true;
        } else if (!t.url.isEmpty() && t.singleton) {
            const QString key = t.url.toString();
            QObject *instance = data->singletons.value(key).data();
            if (!instance) {
                QQmlComponent component(engine, t.url);
                instance = component.create();
                if (instance) {
                    instance->setParent(engine);
                    data->singletons.insert(key, instance);
                } else {
                    data->warn(QQmlError());
                    for (const QQmlError &e : component.errors())
                        data->warn(e);
                }
            }
            v = rt.wrapQObject(instance);
            found = instance != nullptr;
        } else if (t.cpp && t.cpp->metaObject) {
            // The enumerations of a C++ type.
            Object *o = rt.newObject();
            const QMetaObject *mo = t.cpp->metaObject;
            for (int i = 0; i < mo->enumeratorCount(); ++i) {
                const QMetaEnum e = mo->enumerator(i);
                for (int k = 0; k < e.keyCount(); ++k)
                    o->setOwn(QString::fromLatin1(e.key(k)), Value::number(e.value(k)));
            }
            v = Value::object(o);
            found = true;
        } else if (!t.url.isEmpty() || t.inlineNode) {
            // The enumerations declared at the root of a QML type.
            const QmlObjectNode *root = t.inlineNode;
            std::shared_ptr<const ProgramData> program;
            if (!root) {
                QList<QQmlError> errors;
                program = data->load(t.url, &errors);
                root = program ? program->qml.root : nullptr;
            }
            Object *o = rt.newObject();
            if (root)
                for (const QtJSPrivate::QmlEnumDecl *e : root->enums)
                    for (qsizetype k = 0; k < e->keys.size(); ++k)
                        o->setOwn(e->keys.at(k), Value::number(e->values.value(k)));
            v = Value::object(o);
            found = true;
        }
        if (!found) {
            misses.insert(name);
            return false;
        }
        cache.insert(name, v);
        *out = v;
        return true;
    }

    bool getOwn(Runtime &rt, const QString &key, const Value &, Value *out) override { return lookup(rt, key, out); }
    bool hasOwn(Runtime &rt, const QString &key) override {
        Value v;
        return lookup(rt, key, &v);
    }
    bool setOwn(Runtime &rt, const QString &key, const Value &, const Value &) override {
        rt.throwTypeError(QStringLiteral("cannot assign to the type '%1'").arg(key));
    }
    void markChildren(std::vector<QtJSPrivate::Cell *> *stack) const override {
        Object::markChildren(stack);
        for (const Value &v : cache)
            v.mark(stack);
    }

    std::shared_ptr<Imports> imports;
    QQmlEngine *engine;
    EngineData *data;
    QString qualifier;
    QHash<QString, Value> cache;
    QSet<QString> misses;
};

} // namespace

// ----------------------------------------------------------------------------------------------------------------

Creator::Creator(QQmlEngine *engine, const Document &document)
    : m_engine(engine), m_data(EngineData::of(engine)), m_doc(document) {
    m_data->creators.insert(this);
}

Creator::~Creator() {
    if (EngineData *d = EngineData::of(m_engine.data()))
        d->creators.remove(this);
}

QString Creator::location(int line) const { return m_doc.url.toString() + u':' + QString::number(line); }

void Creator::fail(int line, const QString &message) {
    QQmlError e;
    e.setUrl(m_doc.url);
    e.setLine(line);
    e.setDescription(message);
    errors.append(e);
}

Creator::TypeRef Creator::resolveType(const QString &name, int line) {
    TypeRef r;
    if (!m_imports) {
        m_imports = std::make_shared<Imports>(m_data, m_doc);
        m_imports->prepare(&errors);
    }
    const Imports::Type t = m_imports->resolve(name);
    if (!t.found()) {
        fail(line, QStringLiteral("%1 is not a type").arg(name));
        return r;
    }
    if (t.singleton) {
        fail(line, QStringLiteral("%1 is a singleton and cannot be made").arg(name));
        return r;
    }
    r.cpp = t.cpp;
    r.isComponent = t.isComponent;
    r.inlineNode = t.inlineNode;
    if (!t.url.isEmpty()) {
        r.url = t.url;
        QList<QQmlError> loadErrors;
        r.program = m_data->load(t.url, &loadErrors);
        errors.append(loadErrors);
        if (!r.program)
            r.url.clear();
    }
    if (!r.cpp && !r.program && !r.inlineNode && !r.isComponent)
        fail(line, QStringLiteral("%1 could not be loaded").arg(name));
    return r;
}

Env *Creator::contextScopeEnv(QQmlContext *context, QObject *root) {
    // ids and context properties, then the root object of the component, then the types of the document,
    // then the parent context.
    Runtime *rt = m_data->rt;
    Env *types = rt->heap.make<Env>(m_data->contextEnv(context->parentContext()));
    if (!m_typeScope) {
        if (!m_imports) {
            m_imports = std::make_shared<Imports>(m_data, m_doc);
            m_imports->prepare(&errors);
        }
        m_typeScope = rt->heap.make<TypeScope>(m_imports, m_engine, m_data);
    }
    types->object = m_typeScope;
    Env *rootEnv = rt->heap.make<Env>(types);
    rootEnv->object = rt->wrapQObject(root).asObject();
    Env *ctx = rt->heap.make<Env>(rootEnv);
    ctx->object = m_data->contextScope(context);
    m_data->setContextEnv(context, ctx);
    return ctx;
}

QObject *Creator::create(const QmlObjectNode *node, QQmlContext *context, const QVariantMap &initialProperties) {
    if (!node)
        node = m_doc.program ? m_doc.program->qml.root : nullptr;
    if (!node) {
        fail(0, QStringLiteral("the document has no object"));
        return nullptr;
    }
    if (!m_imports) {
        m_imports = std::make_shared<Imports>(m_data, m_doc);
        m_imports->prepare(&errors);
        if (!errors.isEmpty())
            return nullptr;
    }
    QObject *root = nullptr;
    QObject *o = instantiate(node, context, nullptr, &root);
    if (!o || !errors.isEmpty())
        return o;
    for (auto it = initialProperties.cbegin(); it != initialProperties.cend(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();
        QPointer<QObject> target = o;
        m_initial.push_back(
            Pending{o, [this, target, name, value] {
                        if (!target)
                            return;
                        m_data->removeBinding(target.data(), name);
                        QString error;
                        if (!m_data->writeProperty(target.data(), name, m_data->rt->fromVariant(value), &error))
                            fail(0, error);
                    }});
    }
    return o;
}

QObject *Creator::instantiate(const QmlObjectNode *node, QQmlContext *context, QObject *parent, QObject **root) {
    const TypeRef type = resolveType(node->typeName, node->line);
    if (!type.cpp && !type.program && !type.inlineNode && !type.isComponent)
        return nullptr;
    Runtime *rt = m_data->rt;
    QObject *o = nullptr;
    if (type.isComponent) {
        // "Component { Item {} }": a component that makes its one object later, in this context.
        auto *c = new QQmlComponent(m_engine);
        const QmlObjectNode *body = node->children.empty() ? nullptr : node->children.front();
        if (node->children.size() > 1)
            fail(node->line, QStringLiteral("a Component holds a single object"));
        c->m_program = m_doc.program;
        c->m_root = body;
        c->m_url = m_doc.url;
        c->m_creationContext = context;
        c->setStatus(body ? QQmlComponent::Ready : QQmlComponent::Error);
        o = c;
        QQmlEngine::setContextForObject(o, context);
    } else if (type.cpp) {
        if (!type.cpp->create) {
            fail(node->line, QStringLiteral("%1 cannot be made: %2").arg(node->typeName, type.cpp->noCreationReason));
            return nullptr;
        }
        o = type.cpp->create();
        QQmlEngine::setContextForObject(o, context);
    } else {
        // A type written in QML: its document makes the object, in a context of its own.
        const bool inlineType = type.inlineNode != nullptr;
        if (m_depth > 64) {
            fail(node->line, QStringLiteral("%1 is nested too deeply, or contains itself").arg(node->typeName));
            return nullptr;
        }
        Creator *inner = this;
        if (!inlineType) {
            m_owned.push_back(std::make_unique<Creator>(m_engine, Document{type.program, type.url}));
            inner = m_owned.back().get();
            inner->m_depth = m_depth + 1;
        }
        auto *innerContext = new QQmlContext(inlineType ? context : m_engine->rootContext());
        innerContext->setBaseUrl(inlineType ? context->baseUrl() : type.url);
        QObject *innerRoot = nullptr;
        ++m_depth;
        o = inner->instantiate(inlineType ? type.inlineNode : type.program->qml.root, innerContext, nullptr,
                               &innerRoot);
        --m_depth;
        if (inner != this)
            errors.append(inner->errors);
        if (!o) {
            delete innerContext;
            return nullptr;
        }
        innerContext->setParent(o);
    }
    if (!*root)
        *root = o;
    // The scope of the object: its own names, then those of its context.
    Env *ctxEnv = m_data->contextEnv(context);
    if (*root == o)
        ctxEnv = contextScopeEnv(context, o);
    Env *scope = rt->heap.make<Env>(ctxEnv);
    scope->object = rt->wrapQObject(o).asObject();
    ObjectData *d = m_data->data(o, true);
    if (!d->context)
        d->context = context;
    if (!d->scope)
        d->scope = scope;
    if (!node->id.isEmpty())
        context->m_ids.insert(node->id, o);
    if (parent)
        addToDefault(parent, o, node->line);
    if (auto *status = dynamic_cast<QQmlParserStatus *>(o); status && !type.program && !type.inlineNode) {
        status->classBegin();
        m_created.push_back(o);
    } else if (!type.program && !type.inlineNode) {
        m_created.push_back(o);
    }
    if (!type.isComponent)
        applyMembers(o, node, context, scope, *root);
    else
        for (const QmlBinding *b : node->bindings)
            applyBinding(o, b, context, scope, *root);
    return o;
}

void Creator::addToDefault(QObject *parent, QObject *child, int line) {
    Runtime *rt = m_data->rt;
    ObjectData *d = m_data->objects.value(parent);
    QString name = d ? d->defaultProperty : QString();
    if (name.isEmpty()) {
        const QMetaObject *mo = parent->metaObject();
        const int i = mo->indexOfClassInfo("DefaultProperty");
        if (i >= 0)
            name = QString::fromLatin1(mo->classInfo(i).value());
    }
    if (!child->parent())
        child->setParent(parent);
    if (name.isEmpty())
        return;
    if (d && d->property(name)) {
        DeclaredProperty *p = d->property(name);
        if (p->type.startsWith(QLatin1String("list<")) || p->type == QLatin1String("var")) {
            Values items;
            if (p->value.isObject() && p->value.asObject()->cls == Object::Class::Array)
                items = p->value.asObject()->elements;
            items.push_back(rt->wrapQObject(child));
            m_data->setDeclared(parent, name, Value::object(rt->newArray(items)), true);
        } else {
            m_data->setDeclared(parent, name, rt->wrapQObject(child), true);
        }
        return;
    }
    const QMetaObject *mo = parent->metaObject();
    const int index = mo->indexOfProperty(name.toUtf8().constData());
    if (index < 0) {
        fail(line, QStringLiteral("the default property \"%1\" of %2 does not exist").arg(name, typeName(parent)));
        return;
    }
    const QMetaProperty prop = mo->property(index);
    if (QByteArray(prop.typeName()).startsWith("QQmlListProperty<")) {
        QVariant v = prop.read(parent);
        auto *list = static_cast<QQmlListProperty<QObject> *>(v.data());
        if (list && list->append)
            list->append(list, child);
        return;
    }
    QString error;
    if (!m_data->writeProperty(parent, name, rt->wrapQObject(child), &error))
        fail(line, error);
}

void Creator::applyMembers(QObject *o, const QmlObjectNode *node, QQmlContext *context, Env *scope, QObject *root) {
    Runtime *rt = m_data->rt;
    ObjectData *d = m_data->data(o, true);
    for (const QtJSPrivate::QmlEnumDecl *e : node->enums) {
        QHash<QString, int> values;
        for (qsizetype k = 0; k < e->keys.size(); ++k)
            values.insert(e->keys.at(k), e->values.value(k));
        d->enums.insert(e->name, values);
    }
    for (const QtJSPrivate::QmlPropertyDecl *decl : node->properties) {
        if (decl->type.isEmpty())
            continue; // "required name" on a property that exists
        DeclaredProperty p;
        p.name = decl->name;
        p.type = decl->type;
        p.readonly = decl->readonly;
        p.alias = decl->alias;
        p.value = defaultValue(*rt, decl->type);
        if (o->metaObject()->indexOfProperty(decl->name.toUtf8().constData()) >= 0) {
            fail(decl->line, QStringLiteral("the property \"%1\" already exists").arg(decl->name));
            continue;
        }
        m_data->declareProperty(o, p);
        if (decl->isDefault)
            d->defaultProperty = decl->name;
        if (!decl->value)
            continue;
        if (decl->alias) {
            // The target is an id, or a property of one: resolved when all ids are known.
            const Node *e = decl->value->expression;
            QString id, property;
            if (e && e->kind == Node::Kind::Identifier) {
                id = e->str;
            } else if (e && e->kind == Node::Kind::Member && !e->b && e->a->kind == Node::Kind::Identifier) {
                id = e->a->str;
                property = e->str;
            } else {
                fail(decl->line, QStringLiteral("an alias must refer to an id or to a property of one"));
                continue;
            }
            QPointer<QObject> owner = o;
            QPointer<QQmlContext> ctx = context;
            const QString name = decl->name;
            const int line = decl->line;
            m_aliases.push_back(Pending{o, [this, owner, ctx, name, id, property, line] {
                                            if (!owner || !ctx)
                                                return;
                                            QObject *target = ctx->m_ids.value(id).data();
                                            if (!target) {
                                                fail(line, QStringLiteral("alias target \"%1\" not found").arg(id));
                                                return;
                                            }
                                            resolveAlias(owner.data(), name, target, property);
                                        }});
            continue;
        }
        applyBinding(o, decl->value, context, scope, root);
    }
    for (const QtJSPrivate::QmlSignalDecl *s : node->signalDecls)
        m_data->declareSignal(o, s->name, s->parameterNames);
    auto *connections = qobject_cast<QQmlConnections *>(o);
    for (const QtJSPrivate::FunctionNode *f : node->functions) {
        Function *fn = QtJSPrivate::makeClosure(*rt, f, m_doc.program, scope);
        if (connections && isHandlerName(f->name)) {
            Handler h;
            h.function = fn;
            h.thisObject = rt->wrapQObject(o);
            connections->handlers.emplace_back(lowerFirst(f->name.mid(2)), h);
            continue;
        }
        d->methods.insert(f->name, fn);
    }
    if (connections)
        connections->location = location(node->line);
    for (const QmlObjectNode *child : node->children)
        instantiate(child, context, o, &root);
    for (const QmlBinding *b : node->bindings)
        applyBinding(o, b, context, scope, root);
}

void Creator::resolveAlias(QObject *owner, const QString &name, QObject *target, const QString &property) {
    ObjectData *d = m_data->objects.value(owner);
    DeclaredProperty *p = d ? d->property(name) : nullptr;
    if (!p)
        return;
    p->aliasObject = target;
    p->aliasProperty = property;
    if (property.isEmpty())
        return;
    // A change of the target is a change of the alias.
    QPointer<QObject> guard = owner;
    EngineData *data = m_data;
    const auto notify = [data, guard, name] {
        if (!guard)
            return;
        Value now;
        if (data->getDeclared(guard.data(), name, &now))
            data->mirror(guard.data(), name, now);
        data->changed(EngineData::key(guard.data(), name));
        data->emitDeclared(guard.data(), name + QStringLiteral("Changed"), {});
    };
    notify();
    if (ObjectData *td = m_data->objects.value(target); td && td->property(property)) {
        Handler h;
        h.function = m_data->rt->newNative(QString(), 0, [notify](Runtime &, const Value &, const Values &, Object *) {
            notify();
            return Value();
        });
        td->handlers[property + QStringLiteral("Changed")].push_back(h);
        return;
    }
    const QMetaObject *mo = target->metaObject();
    const int index = mo->indexOfProperty(property.toUtf8().constData());
    if (index < 0) {
        fail(0, QStringLiteral("alias target property \"%1\" not found").arg(property));
        return;
    }
    const QMetaProperty prop = mo->property(index);
    if (prop.hasNotifySignal())
        d->connections.push_back(QtPrivate::Access::connectFunction(target, prop.notifySignal(), owner,
                                                                    [notify](void *const *) { notify(); }));
}

void Creator::applyBinding(QObject *o, const QmlBinding *b, QQmlContext *context, Env *scope, QObject *root) {
    Runtime *rt = m_data->rt;
    ObjectData *d = m_data->data(o, true);
    const QString &name = b->name;
    if (name == QLatin1String("Component.onCompleted") || name == QLatin1String("Component.onDestruction")) {
        Handler h;
        h.node = b->function;
        h.program = m_doc.program;
        h.scope = scope;
        h.thisObject = rt->wrapQObject(o);
        // Run through callHandler, so that "onCompleted: function() {}" works too.
        Function *call = rt->newNative(name, 0, [this, h](Runtime &, const Value &, const Values &, Object *) {
            m_data->callHandler(h, {}, {});
            return Value();
        });
        (name.endsWith(QLatin1String("Completed")) ? d->onCompleted : d->onDestruction).push_back(call);
        return;
    }
    const qsizetype dot = name.indexOf(u'.');
    if (dot > 0 && name.at(0).isUpper()) {
        m_data->warn(location(b->line), QStringLiteral("attached properties are not supported yet: %1").arg(name));
        return;
    }
    if (b->onAssignment) {
        m_data->warn(location(b->line),
                     QStringLiteral("\"%1 on %2\" is not supported yet; it is ignored").arg(b->object->typeName, name));
        return;
    }
    const QString last = name.mid(name.lastIndexOf(u'.') + 1);
    if (isHandlerName(last) && !d->property(last) && o->metaObject()->indexOfProperty(last.toUtf8().constData()) < 0) {
        Handler h;
        h.node = b->function;
        h.program = m_doc.program;
        h.scope = scope;
        h.thisObject = rt->wrapQObject(o);
        QObject *target = o;
        if (dot > 0) {
            target = rt->unwrapQObject(m_data->readProperty(o, name.left(name.lastIndexOf(u'.'))));
            if (!target) {
                fail(b->line, QStringLiteral("cannot assign a handler to \"%1\"").arg(name));
                return;
            }
        }
        const QString signal = lowerFirst(last.mid(2));
        if (auto *connections = qobject_cast<QQmlConnections *>(target);
            connections && signal != QLatin1String("targetChanged") && signal != QLatin1String("enabledChanged")) {
            connections->handlers.emplace_back(signal, h);
            return;
        }
        // Handlers are installed when the component is complete, so that setting the first values does not
        // call them.
        QPointer<QObject> handlerTarget = target;
        const int line = b->line;
        m_handlers.push_back(Pending{target, [this, handlerTarget, signal, h, line] {
                                         QString error;
                                         if (handlerTarget &&
                                             !m_data->connectHandler(handlerTarget.data(), signal, h, &error))
                                             fail(line, error);
                                     }});
        return;
    }
    QPointer<QObject> target = o;
    if (b->object) {
        QObject *child = instantiate(b->object, context, nullptr, &root);
        if (!child)
            return;
        if (!child->parent())
            child->setParent(o);
        // Objects are assigned as they are made, before any binding runs.
        QString error;
        if (!m_data->writeProperty(o, name, rt->wrapQObject(child), &error))
            fail(b->line, error);
        return;
    }
    if (!b->objects.empty()) {
        Values items;
        for (const QmlObjectNode *n : b->objects) {
            QObject *child = instantiate(n, context, nullptr, &root);
            if (!child)
                continue;
            if (!child->parent())
                child->setParent(o);
            items.push_back(rt->wrapQObject(child));
        }
        QString error;
        if (!m_data->writeProperty(o, name, Value::object(rt->newArray(items)), &error))
            fail(b->line, error);
        return;
    }
    Value constant;
    if (b->expression && constantValue(b->expression, &constant)) {
        m_values.push_back(Pending{o, [this, target, name, constant, b] {
                                       QString error;
                                       if (target && !m_data->writeProperty(target.data(), name, constant, &error))
                                           fail(b->line, error);
                                   }});
        return;
    }
    Function *fn = QtJSPrivate::makeClosure(*rt, b->function, m_doc.program, scope);
    keep.push_back(fn);
    const Value self = rt->wrapQObject(o);
    const QString where = location(b->line);
    m_pending.push_back(Pending{o, [this, target, name, fn, self, where] {
                                    if (!target)
                                        return;
                                    auto *binding = new Binding;
                                    binding->target = target;
                                    binding->property = name;
                                    binding->function = fn;
                                    binding->thisObject = self;
                                    binding->location = where;
                                    m_data->setBinding(target.data(), name, binding);
                                }});
}

void Creator::complete() {
    if (m_completed)
        return;
    m_completed = true;
    for (const auto &inner : m_owned) {
        inner->m_completeObjects = false;
        inner->complete();
        errors.append(inner->errors);
    }
    // Aliases, then values, then bindings, then the initial properties, then the handlers.
    for (const std::vector<Pending> *queue : {&m_aliases, &m_values, &m_pending, &m_initial, &m_handlers})
        for (const Pending &p : *queue)
            if (p.object)
                p.apply();
    if (!m_completeObjects)
        return;
    completeObjects();
}

void Creator::completeObjects() {
    // Inner documents' objects are completed with the outer ones, after every binding is set.
    std::vector<QPointer<QObject>> created;
    collectCreated(&created);
    for (const QPointer<QObject> &o : created)
        if (auto *status = dynamic_cast<QQmlParserStatus *>(o.data()))
            status->componentComplete();
    std::vector<Function *> completed;
    for (auto it = created.rbegin(); it != created.rend(); ++it)
        if (ObjectData *d = it->data() ? m_data->objects.value(it->data()) : nullptr)
            completed.insert(completed.end(), d->onCompleted.begin(), d->onCompleted.end());
    for (Function *f : completed) {
        try {
            m_data->rt->call(Value::object(f), Value(), {});
        } catch (const Throw &t) {
            m_data->warn(QString(), m_data->errorText(t.value));
        }
    }
}

void Creator::collectCreated(std::vector<QPointer<QObject>> *out) const {
    for (const auto &inner : m_owned)
        inner->collectCreated(out);
    out->insert(out->end(), m_created.begin(), m_created.end());
}

} // namespace QtQmlPrivate
