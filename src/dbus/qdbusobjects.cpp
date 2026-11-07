// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbuscontext_p.h"
#include "qdbusmarshaller_p.h"
#include "qdbustransport_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtDBus/qdbusabstractadaptor.h>
#include <QtDBus/qdbusmetatype.h>
#include <QtDBus/qdbusvirtualobject.h>

// Exporting objects, dispatching calls to them, introspection, and signal hooks.

namespace {

const char kIntrospectable[] = "org.freedesktop.DBus.Introspectable";
const char kProperties[] = "org.freedesktop.DBus.Properties";
const char kPeer[] = "org.freedesktop.DBus.Peer";

const char kDocType[] = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n";

const char kStandardInterfaces[] = "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
                                   "    <method name=\"Get\">\n"
                                   "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
                                   "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
                                   "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
                                   "    </method>\n"
                                   "    <method name=\"Set\">\n"
                                   "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
                                   "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
                                   "      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
                                   "    </method>\n"
                                   "    <method name=\"GetAll\">\n"
                                   "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
                                   "      <arg name=\"values\" type=\"a{sv}\" direction=\"out\"/>\n"
                                   "    </method>\n"
                                   "    <signal name=\"PropertiesChanged\">\n"
                                   "      <arg name=\"interface_name\" type=\"s\"/>\n"
                                   "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
                                   "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
                                   "    </signal>\n"
                                   "  </interface>\n"
                                   "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                                   "    <method name=\"Introspect\">\n"
                                   "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
                                   "    </method>\n"
                                   "  </interface>\n"
                                   "  <interface name=\"org.freedesktop.DBus.Peer\">\n"
                                   "    <method name=\"Ping\"/>\n"
                                   "    <method name=\"GetMachineId\">\n"
                                   "      <arg name=\"machine_uuid\" type=\"s\" direction=\"out\"/>\n"
                                   "    </method>\n"
                                   "  </interface>\n";

QString escapeXml(const QString &text) {
    QString out;
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '<':
            out += QStringLiteral("&lt;");
            break;
        case '>':
            out += QStringLiteral("&gt;");
            break;
        case '&':
            out += QStringLiteral("&amp;");
            break;
        case '"':
            out += QStringLiteral("&quot;");
            break;
        default:
            out += c;
        }
    }
    return out;
}

// A class and the object whose members of that class are exported.
struct Candidate {
    QObject *target;
    const QMetaObject *cls;
    QString interface;
    bool adaptor;
};

bool isExported(const QMetaMethod &m, QDBusConnection::RegisterOptions options, bool adaptor) {
    if (m.attributes() & QMetaMethod::Cloned)
        return false;
    const bool scriptable = (m.attributes() & QMetaMethod::Scriptable) != 0;
    switch (m.methodType()) {
    case QMetaMethod::Signal:
        return adaptor || (options & (scriptable ? QDBusConnection::ExportScriptableSignals
                                                 : QDBusConnection::ExportNonScriptableSignals));
    case QMetaMethod::Slot:
        if (m.access() != QMetaMethod::Public)
            return false;
        return adaptor || (options & (scriptable ? QDBusConnection::ExportScriptableSlots
                                                 : QDBusConnection::ExportNonScriptableSlots));
    case QMetaMethod::Method:
        if (m.access() != QMetaMethod::Public)
            return false;
        return adaptor || (options & (scriptable ? QDBusConnection::ExportScriptableInvokables
                                                 : QDBusConnection::ExportNonScriptableInvokables));
    case QMetaMethod::Constructor:
        return false;
    }
    return false;
}

bool isExported(const QMetaProperty &p, QDBusConnection::RegisterOptions options, bool adaptor) {
    if (adaptor)
        return true;
    return options & (p.isScriptable() ? QDBusConnection::ExportScriptableProperties
                                       : QDBusConnection::ExportNonScriptableProperties);
}

QList<QDBusAbstractAdaptor *> adaptorsOf(QObject *object) {
    QList<QDBusAbstractAdaptor *> list;
    for (QObject *child : object->children())
        if (auto *a = dynamic_cast<QDBusAbstractAdaptor *>(child))
            list.append(a);
    return list;
}

// The classes that contribute members, most derived first, with the interface of each.
QList<Candidate> candidatesOf(QObject *object, QDBusConnection::RegisterOptions options, const QString &interface) {
    QList<Candidate> list;
    if (options & QDBusConnection::ExportAdaptors) {
        for (QDBusAbstractAdaptor *a : adaptorsOf(object)) {
            const QString name = qDBusInterfaceOf(a->metaObject());
            for (const QMetaObject *mo = a->metaObject(); mo && mo != &QDBusAbstractAdaptor::staticMetaObject;
                 mo = mo->superClass())
                list.append({a, mo, name, true});
        }
    }
    if (options & QDBusConnection::ExportAllContents) {
        for (const QMetaObject *mo = object->metaObject(); mo && mo != &QObject::staticMetaObject;
             mo = mo->superClass())
            list.append({object, mo, interface.isEmpty() ? qDBusInterfaceOf(mo) : interface, false});
    }
    return list;
}

QString argXml(const QByteArray &name, const char *signature, const char *direction) {
    QString s = QStringLiteral("      <arg");
    if (!name.isEmpty())
        s += QStringLiteral(" name=\"%1\"").arg(escapeXml(QString::fromUtf8(name)));
    s += QStringLiteral(" type=\"%1\"").arg(escapeXml(QString::fromLatin1(signature)));
    if (direction)
        s += QStringLiteral(" direction=\"%1\"").arg(QString::fromLatin1(direction));
    return s + QStringLiteral("/>\n");
}

QString membersXml(const QList<Candidate> &classes, QDBusConnection::RegisterOptions options) {
    QString xml;
    for (const Candidate &c : classes) {
        const QMetaObject *mo = c.cls;
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
            const QMetaProperty p = mo->property(i);
            const char *sig = QDBusMetaType::typeToSignature(p.metaType());
            if (!sig || !isExported(p, options, c.adaptor))
                continue;
            const char *access = p.isReadable() && p.isWritable() ? "readwrite" : p.isWritable() ? "write" : "read";
            xml += QStringLiteral("    <property name=\"%1\" type=\"%2\" access=\"%3\"/>\n")
                       .arg(QString::fromLatin1(p.name()), escapeXml(QString::fromLatin1(sig)),
                            QString::fromLatin1(access));
        }
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            const QMetaMethod m = mo->method(i);
            if (!isExported(m, options, c.adaptor))
                continue;
            const QDBusMethodInfo info = QDBusMethodInfo::parse(m);
            if (!info.valid)
                continue;
            const bool isSignal = m.methodType() == QMetaMethod::Signal;
            const QString tag = isSignal ? QStringLiteral("signal") : QStringLiteral("method");
            xml += QStringLiteral("    <%1 name=\"%2\">\n").arg(tag, QString::fromLatin1(m.name()));
            for (qsizetype k = 0; k < info.inTypes.size(); ++k)
                xml += argXml(info.inNames.value(k), QDBusMetaType::typeToSignature(info.inTypes.at(k)),
                              isSignal ? nullptr : "in");
            for (qsizetype k = 0; k < info.outTypes.size(); ++k)
                xml += argXml(info.outNames.value(k), QDBusMetaType::typeToSignature(info.outTypes.at(k)), "out");
            if (!isSignal && std::strcmp(m.tag(), "Q_NOREPLY") == 0)
                xml +=
                    QStringLiteral("      <annotation name=\"org.freedesktop.DBus.Method.NoReply\" value=\"true\"/>\n");
            xml += QStringLiteral("    </%1>\n").arg(tag);
        }
    }
    return xml;
}

// Finds the property `name` of `interface` (any interface if it is empty).
bool findProperty(QObject *object, QDBusConnection::RegisterOptions options, const QString &regInterface,
                  const QString &interface, const QString &name, QObject **target, QMetaProperty *property) {
    const QByteArray latin = name.toLatin1();
    for (const Candidate &c : candidatesOf(object, options, regInterface)) {
        if (!interface.isEmpty() && interface != c.interface)
            continue;
        for (int i = c.cls->propertyOffset(); i < c.cls->propertyCount(); ++i) {
            const QMetaProperty p = c.cls->property(i);
            if (latin == p.name() && isExported(p, options, c.adaptor) &&
                QDBusMetaType::typeToSignature(p.metaType())) {
                *target = c.target;
                *property = p;
                return true;
            }
        }
    }
    return false;
}

QString parentPath(const QString &path) {
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}

QString matchRule(const QString &service, const QString &path, const QString &interface, const QString &member,
                  const QStringList &argumentMatch, const QString &argNamespace) {
    auto quote = [](const QString &v) {
        QString q = v;
        q.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        return QLatin1Char('\'') + q + QLatin1Char('\'');
    };
    QString rule = QStringLiteral("type='signal'");
    if (!service.isEmpty())
        rule += QStringLiteral(",sender=") + quote(service);
    if (!path.isEmpty())
        rule += QStringLiteral(",path=") + quote(path);
    if (!interface.isEmpty())
        rule += QStringLiteral(",interface=") + quote(interface);
    if (!member.isEmpty())
        rule += QStringLiteral(",member=") + quote(member);
    for (qsizetype i = 0; i < argumentMatch.size(); ++i)
        if (!argumentMatch.at(i).isEmpty())
            rule += QStringLiteral(",arg%1=").arg(i) + quote(argumentMatch.at(i));
    if (!argNamespace.isEmpty())
        rule += QStringLiteral(",arg0namespace=") + quote(argNamespace);
    return rule;
}

// Fills the context of the objects that a call runs in, and restores it afterwards.
class ContextScope {
public:
    ContextScope(QObject *a, QObject *b, QDBusContextPrivate *context) : m_a(a), m_b(b != a ? b : nullptr) {
        m_oldA = QDBusContextPrivate::set(m_a, context);
        if (m_b)
            m_oldB = QDBusContextPrivate::set(m_b, context);
    }
    ~ContextScope() {
        QDBusContextPrivate::set(m_a, m_oldA);
        if (m_b)
            QDBusContextPrivate::set(m_b, m_oldB);
    }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;

private:
    QObject *m_a;
    QObject *m_b;
    QDBusContextPrivate *m_oldA = nullptr;
    QDBusContextPrivate *m_oldB = nullptr;
};

// Sends an error if a call that was posted to an object is never delivered.
struct CallToken {
    std::weak_ptr<QDBusConnectionPrivate> connection;
    QDBusMessage message;
    bool handled = false;
    CallToken(std::weak_ptr<QDBusConnectionPrivate> c, const QDBusMessage &m) : connection(std::move(c)), message(m) {}
    ~CallToken() {
        if (handled || !message.isReplyRequired())
            return;
        if (auto d = connection.lock())
            d->send(message.createErrorReply(QDBusError::UnknownObject,
                                             QStringLiteral("The object at %1 was destroyed").arg(message.path())));
    }
    CallToken(const CallToken &) = delete;
    CallToken &operator=(const CallToken &) = delete;
};

} // namespace

QString qDBusInterfaceOf(const QMetaObject *mo) {
    const int index = mo->indexOfClassInfo("D-Bus Interface");
    if (index >= 0)
        return QString::fromUtf8(mo->classInfo(index).value());
    QString name = QString::fromLatin1(mo->className());
    name.replace(QStringLiteral("::"), QStringLiteral("."));
    return QStringLiteral("local.") + name;
}

QVariant qDBusAdaptArgument(const QVariant &in, QMetaType target, bool *ok) {
    *ok = true;
    if (target == QMetaType::fromType<QVariant>()) {
        if (in.metaType() == QMetaType::fromType<QDBusVariant>())
            return in.value<QDBusVariant>().variant();
        return in;
    }
    if (in.metaType() == target)
        return in;
    if (in.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QVariant out(target, nullptr);
        *ok = QDBusMetaType::demarshall(in.value<QDBusArgument>(), target, out.data());
        return out;
    }
    if (target == QMetaType::fromType<QDBusVariant>())
        return QVariant::fromValue(QDBusVariant(in));
    QVariant converted = in;
    if (converted.convert(target))
        return converted;
    *ok = false;
    return QVariant();
}

// ---- QDBusMethodInfo -----------------------------------------------------------------------

QDBusMethodInfo QDBusMethodInfo::parse(const QMetaMethod &method) {
    QDBusMethodInfo info;
    info.method = method;
    const QList<QByteArray> types = method.parameterTypes();
    const QList<QByteArray> names = method.parameterNames();
    const bool isSignal = method.methodType() == QMetaMethod::Signal;
    const QMetaType ret = method.returnMetaType();
    if (ret.isValid() && ret.id() != QMetaType::Void) {
        if (isSignal || !QDBusMetaType::typeToSignature(ret))
            return info;
        info.outTypes.append(ret);
        info.outNames.append(QByteArray());
        info.outSignature += QDBusMetaType::typeToSignature(ret);
        info.hasReturn = true;
    } else if (!ret.isValid() && std::strcmp(method.typeName(), "void") != 0 && method.typeName()[0]) {
        return info;
    }
    bool seenOutput = false;
    for (qsizetype i = 0; i < types.size(); ++i) {
        const QByteArray &t = types.at(i);
        if (t == "QDBusMessage") {
            if (info.wantsMessage ||
                (i != types.size() - 1 && !(i + 1 < types.size() && types.at(i + 1).endsWith('&'))))
                return info;
            info.wantsMessage = true;
            info.kinds.append('m');
            continue;
        }
        if (t.endsWith('&')) {
            if (isSignal)
                return info;
            const QMetaType mt = QMetaType::fromName(t.chopped(1));
            const char *sig = QDBusMetaType::typeToSignature(mt);
            if (!sig)
                return info;
            seenOutput = true;
            info.outTypes.append(mt);
            info.outNames.append(names.value(i));
            info.outSignature += sig;
            info.kinds.append('o');
            continue;
        }
        if (seenOutput || info.wantsMessage)
            return info;
        QMetaType mt = method.parameterMetaType(static_cast<int>(i));
        if (!mt.isValid())
            mt = QMetaType::fromName(t);
        const char *sig = QDBusMetaType::typeToSignature(mt);
        if (!sig)
            return info;
        info.inTypes.append(mt);
        info.inNames.append(names.value(i));
        info.inSignature += sig;
        info.kinds.append('i');
    }
    info.valid = true;
    return info;
}

QDBusMethodInfo QDBusMethodInfo::fromMember(QObject *receiver, const char *member) {
    if (!receiver || !member || !*member)
        return QDBusMethodInfo();
    const char *name = (*member >= '0' && *member <= '2') ? member + 1 : member;
    const QByteArray sig = QMetaObject::normalizedSignature(name);
    const int index = receiver->metaObject()->indexOfMethod(sig.constData());
    if (index < 0)
        return QDBusMethodInfo();
    return parse(receiver->metaObject()->method(index));
}

bool QDBusMethodInfo::invoke(QObject *object, const QDBusMessage &message, QList<QVariant> *outputs,
                             QDBusError *error) const {
    const QList<QVariant> args = message.arguments();
    if (args.size() < inTypes.size()) {
        *error = QDBusError(QDBusError::InvalidArgs, QStringLiteral("Too few arguments"));
        return false;
    }
    const auto count = static_cast<std::size_t>(kinds.size());
    std::vector<QVariant> storage;
    storage.reserve(count + 1);
    std::vector<void *> argv(count + 1, nullptr);
    std::vector<std::pair<QMetaType, std::size_t>> outs; // the type and the storage index
    const QMetaType variantType = QMetaType::fromType<QVariant>();
    auto slotFor = [&storage, &variantType](QMetaType t) -> void * {
        if (t == variantType) {
            storage.emplace_back();
            return &storage.back();
        }
        storage.emplace_back(t, nullptr);
        return storage.back().data();
    };
    if (hasReturn) {
        argv[0] = slotFor(outTypes.first());
        outs.emplace_back(outTypes.first(), storage.size() - 1);
    }
    QDBusMessage copy = message;
    qsizetype in = 0;
    qsizetype out = hasReturn ? 1 : 0;
    for (std::size_t i = 0; i < count; ++i) {
        switch (kinds.at(static_cast<qsizetype>(i))) {
        case 'i': {
            bool ok = false;
            const QMetaType t = inTypes.at(in);
            QVariant v = qDBusAdaptArgument(args.at(in), t, &ok);
            if (!ok) {
                *error = QDBusError(QDBusError::InvalidArgs, QStringLiteral("Argument %1 cannot be converted to %2")
                                                                 .arg(in + 1)
                                                                 .arg(QString::fromLatin1(t.name())));
                return false;
            }
            storage.push_back(std::move(v));
            argv[i + 1] = t == variantType ? static_cast<void *>(&storage.back()) : storage.back().data();
            ++in;
            break;
        }
        case 'o':
            argv[i + 1] = slotFor(outTypes.at(out));
            outs.emplace_back(outTypes.at(out), storage.size() - 1);
            ++out;
            break;
        default:
            argv[i + 1] = &copy;
            break;
        }
    }
    method.enclosingMetaObject()->d.static_metacall(object, QMetaObject::InvokeMetaMethod, method.relativeMethodIndex(),
                                                    argv.data());
    if (outputs) {
        for (const auto &o : outs) {
            const QVariant &v = storage.at(o.second);
            outputs->append(o.first == variantType ? QVariant::fromValue(QDBusVariant(v)) : v);
        }
    }
    return true;
}

// ---- registering objects -------------------------------------------------------------------

void QDBusConnectionPrivate::installRelays(ObjectRegistration &reg) {
    std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
    const QString path = reg.path;
    auto relay = [&reg, weak, path](QObject *sender, const QMetaMethod &signal, const QString &interface) {
        const QDBusMethodInfo info = QDBusMethodInfo::parse(signal);
        if (!info.valid || info.wantsMessage)
            return;
        const QList<QMetaType> types = info.inTypes;
        const QString name = QString::fromLatin1(signal.name());
        reg.relays.push_back(QtPrivate::Access::connectFunction(
            sender, signal, nullptr, [weak, path, interface, name, types](void *const *argv) {
                const auto self = weak.lock();
                if (!self || !self->isConnected())
                    return;
                QDBusMessage m = QDBusMessage::createSignal(path, interface, name);
                for (qsizetype i = 0; i < types.size(); ++i) {
                    const QMetaType t = types.at(i);
                    if (t == QMetaType::fromType<QVariant>())
                        m << QVariant::fromValue(QDBusVariant(*static_cast<const QVariant *>(argv[i])));
                    else
                        m << QVariant(t, argv[i]);
                }
                self->send(m);
            }));
    };
    QObject *object = reg.object;
    for (const Candidate &c : candidatesOf(object, reg.options, reg.interface)) {
        for (int i = c.cls->methodOffset(); i < c.cls->methodCount(); ++i) {
            const QMetaMethod m = c.cls->method(i);
            if (m.methodType() == QMetaMethod::Signal && isExported(m, reg.options, c.adaptor))
                relay(c.target, m, c.interface);
        }
    }
}

bool QDBusConnectionPrivate::registerObject(const QString &path, const QString &interface, QObject *object,
                                            QDBusConnection::RegisterOptions options) {
    if (!object || !QDBusUtil::isValidObjectPath(path) ||
        (!interface.isEmpty() && !QDBusUtil::isValidInterfaceName(interface)))
        return false;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        if (m_objects.count(path))
            return false;
    }
    ObjectRegistration reg;
    reg.path = path;
    reg.object = object;
    reg.interface = interface;
    reg.options = options;
    std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
    reg.guard = QDBusTargetGuard::create(object, [weak, path, object] {
        if (auto self = weak.lock())
            self->removeRegistrations(
                [&](const ObjectRegistration &r) { return r.path == path && r.object == object; });
    });
    installRelays(reg);
    std::vector<QMetaObject::Connection> undo;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        if (m_objects.count(path)) {
            undo.swap(reg.relays);
        } else {
            m_objects.emplace(path, std::move(reg));
            return true;
        }
    }
    for (const auto &c : undo)
        QObject::disconnect(c);
    return false;
}

bool QDBusConnectionPrivate::registerVirtualObject(const QString &path, QDBusVirtualObject *object,
                                                   QDBusConnection::VirtualObjectRegisterOption options) {
    if (!object || !QDBusUtil::isValidObjectPath(path))
        return false;
    ObjectRegistration reg;
    reg.path = path;
    reg.object = object;
    reg.virtualObject = object;
    reg.subPath = options == QDBusConnection::SubPath;
    std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
    QObject *key = object;
    reg.guard = QDBusTargetGuard::create(object, [weak, path, key] {
        if (auto self = weak.lock())
            self->removeRegistrations([&](const ObjectRegistration &r) { return r.path == path && r.object == key; });
    });
    std::lock_guard<std::mutex> lock(m_tableMutex);
    if (m_objects.count(path))
        return false;
    m_objects.emplace(path, std::move(reg));
    return true;
}

void QDBusConnectionPrivate::removeRegistrations(const std::function<bool(const ObjectRegistration &)> &match) {
    std::vector<ObjectRegistration> removed;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (auto it = m_objects.begin(); it != m_objects.end();) {
            if (match(it->second)) {
                removed.push_back(std::move(it->second));
                it = m_objects.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const ObjectRegistration &r : removed)
        for (const auto &c : r.relays)
            QObject::disconnect(c);
}

void QDBusConnectionPrivate::unregisterObject(const QString &path, QDBusConnection::UnregisterMode mode) {
    const QString prefix = path == QLatin1String("/") ? path : path + QLatin1Char('/');
    removeRegistrations([&](const ObjectRegistration &r) {
        return r.path == path || (mode == QDBusConnection::UnregisterTree && r.path.startsWith(prefix));
    });
}

QObject *QDBusConnectionPrivate::objectRegisteredAt(const QString &path) {
    QObject *object = nullptr;
    QString rest;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (QString p = path;; p = parentPath(p)) {
            const auto it = m_objects.find(p);
            if (it != m_objects.end()) {
                if (p != path && !(it->second.options & QDBusConnection::ExportChildObjects))
                    return nullptr;
                object = it->second.guard->target();
                rest = path.mid(p.size());
                break;
            }
            if (p == QLatin1String("/"))
                return nullptr;
        }
    }
    const QStringList parts = rest.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QObject *next = nullptr;
        if (object)
            for (QObject *child : object->children())
                if (child->objectName() == part) {
                    next = child;
                    break;
                }
        object = next;
    }
    return object;
}

QStringList QDBusConnectionPrivate::childNodes(const QString &path) {
    const QString prefix = path == QLatin1String("/") ? path : path + QLatin1Char('/');
    QStringList names;
    std::lock_guard<std::mutex> lock(m_tableMutex);
    for (const auto &e : m_objects) {
        if (!e.first.startsWith(prefix) || e.first == path)
            continue;
        const QString name = e.first.mid(prefix.size()).section(QLatin1Char('/'), 0, 0);
        if (!name.isEmpty() && !names.contains(name))
            names.append(name);
    }
    return names;
}

QString QDBusConnectionPrivate::introspect(QObject *object, QDBusConnection::RegisterOptions options,
                                           const QString &interface, const QString &path,
                                           const QStringList &childNames) {
    (void)path;
    QString xml = QString::fromLatin1(kDocType) + QStringLiteral("<node>\n");
    if (object) {
        QList<Candidate> group;
        QString current;
        auto emitGroup = [&] {
            if (group.isEmpty())
                return;
            const QObject *target = group.first().target;
            const QMetaObject *mo = target->metaObject();
            const int raw = group.first().adaptor ? mo->indexOfClassInfo("D-Bus Introspection") : -1;
            if (raw >= 0) {
                xml += QString::fromUtf8(mo->classInfo(raw).value());
            } else {
                const QString members = membersXml(group, options);
                if (!members.isEmpty() || group.first().adaptor)
                    xml += QStringLiteral("  <interface name=\"%1\">\n").arg(escapeXml(current)) + members +
                           QStringLiteral("  </interface>\n");
            }
            group.clear();
        };
        for (const Candidate &c : candidatesOf(object, options, interface)) {
            if (!group.isEmpty() && (c.interface != current || c.target != group.first().target))
                emitGroup();
            current = c.interface;
            group.append(c);
        }
        emitGroup();
    }
    xml += QString::fromLatin1(kStandardInterfaces);
    for (const QString &name : childNames)
        xml += QStringLiteral("  <node name=\"%1\"/>\n").arg(escapeXml(name));
    return xml + QStringLiteral("</node>\n");
}

// ---- dispatching calls ---------------------------------------------------------------------

void QDBusConnectionPrivate::routeMethodCall(const QDBusMessage &message, bool inCallingThread) {
    const QString interface = message.interface();
    const QString member = message.member();
    auto replyError = [this, &message](QDBusError::ErrorType type, const QString &text) {
        if (message.isReplyRequired())
            send(message.createErrorReply(type, text));
    };
    if (interface == QLatin1String(kPeer)) {
        if (member == QLatin1String("Ping") && message.signature().isEmpty()) {
            if (message.isReplyRequired())
                send(message.createReply());
        } else if (member == QLatin1String("GetMachineId") && message.signature().isEmpty()) {
            if (message.isReplyRequired())
                send(message.createReply(QString::fromLatin1(QDBusTransport::machineId())));
        } else {
            replyError(QDBusError::UnknownMethod,
                       QStringLiteral("No such method '%1' in interface '%2'").arg(member, interface));
        }
        return;
    }

    const QString path = message.path();
    ObjectRegistration reg;
    QString relative;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (QString p = path;; p = parentPath(p)) {
            const auto it = m_objects.find(p);
            if (it != m_objects.end()) {
                const ObjectRegistration &r = it->second;
                const bool covers =
                    p == path || (r.virtualObject ? r.subPath : (r.options & QDBusConnection::ExportChildObjects) != 0);
                if (covers) {
                    reg.path = r.path;
                    reg.object = r.object;
                    reg.virtualObject = r.virtualObject;
                    reg.interface = r.interface;
                    reg.options = r.options;
                    reg.subPath = r.subPath;
                    reg.guard = r.guard;
                    relative = path.mid(p.size());
                    found = true;
                }
                break;
            }
            if (p == QLatin1String("/"))
                break;
        }
    }
    if (!found) {
        const QStringList children = childNodes(path);
        const bool wantsXml = (interface.isEmpty() || interface == QLatin1String(kIntrospectable)) &&
                              member == QLatin1String("Introspect") && message.signature().isEmpty();
        if (wantsXml && (!children.isEmpty() || path == QLatin1String("/"))) {
            if (message.isReplyRequired())
                send(message.createReply(introspect(nullptr, {}, QString(), path, children)));
            return;
        }
        replyError(QDBusError::UnknownObject, QStringLiteral("No such object path '%1'").arg(path));
        return;
    }

    QObject *target = reg.guard->target();
    if (inCallingThread && target && target->thread() == QThread::currentThread()) {
        deliverCall(reg, relative, message);
        return;
    }
    auto token = std::make_shared<CallToken>(weak_from_this(), message);
    std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
    reg.guard->post([weak, reg, relative, token] {
        token->handled = true;
        if (auto self = weak.lock())
            self->deliverCall(reg, relative, token->message);
    });
}

void QDBusConnectionPrivate::deliverCall(const ObjectRegistration &reg, const QString &relative,
                                         const QDBusMessage &message) {
    auto replyError = [this, &message](QDBusError::ErrorType type, const QString &text) {
        if (message.isReplyRequired())
            send(message.createErrorReply(type, text));
    };
    QObject *object = reg.guard->target();
    if (!object) {
        replyError(QDBusError::UnknownObject, QStringLiteral("No such object path '%1'").arg(message.path()));
        return;
    }
    const QString interface = message.interface();
    const QString member = message.member();
    const QByteArray signature = message.signature().toLatin1();
    const bool wantsXml = (interface.isEmpty() || interface == QLatin1String(kIntrospectable)) &&
                          member == QLatin1String("Introspect") && signature.isEmpty();
    const QDBusConnection connection = wrap(shared_from_this());

    if (reg.virtualObject) {
        if (wantsXml) {
            const QString inner = reg.virtualObject->introspect(message.path());
            QString xml = QString::fromLatin1(kDocType) + QStringLiteral("<node>\n") + inner +
                          QString::fromLatin1(kStandardInterfaces) + QStringLiteral("</node>\n");
            if (message.isReplyRequired())
                send(message.createReply(xml));
            return;
        }
        if (!reg.virtualObject->handleMessage(message, connection))
            replyError(QDBusError::UnknownMethod,
                       QStringLiteral("No such method '%1' at object path '%2'").arg(member, message.path()));
        return;
    }

    const QStringList parts = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QObject *next = nullptr;
        for (QObject *child : object->children())
            if (child->objectName() == part) {
                next = child;
                break;
            }
        if (!next) {
            replyError(QDBusError::UnknownObject, QStringLiteral("No such object path '%1'").arg(message.path()));
            return;
        }
        object = next;
    }

    if (wantsXml) {
        QStringList children = childNodes(message.path());
        if (reg.options & QDBusConnection::ExportChildObjects)
            for (QObject *child : object->children())
                if (!child->objectName().isEmpty() && !dynamic_cast<QDBusAbstractAdaptor *>(child) &&
                    !children.contains(child->objectName()))
                    children.append(child->objectName());
        if (message.isReplyRequired())
            send(message.createReply(introspect(object, reg.options, reg.interface, message.path(), children)));
        return;
    }

    if (interface == QLatin1String(kProperties)) {
        const QList<QVariant> args = message.arguments();
        QObject *target = nullptr;
        QMetaProperty property;
        if (member == QLatin1String("Get") && signature == "ss") {
            if (!findProperty(object, reg.options, reg.interface, args.at(0).toString(), args.at(1).toString(), &target,
                              &property) ||
                !property.isReadable()) {
                replyError(QDBusError::UnknownProperty, QStringLiteral("No such property '%1' in interface '%2'")
                                                            .arg(args.at(1).toString(), args.at(0).toString()));
                return;
            }
            const QVariant value = property.read(target);
            if (message.isReplyRequired())
                send(message.createReply(QVariant::fromValue(QDBusVariant(value))));
            return;
        }
        if (member == QLatin1String("Set") && signature == "ssv") {
            if (!findProperty(object, reg.options, reg.interface, args.at(0).toString(), args.at(1).toString(), &target,
                              &property)) {
                replyError(QDBusError::UnknownProperty, QStringLiteral("No such property '%1' in interface '%2'")
                                                            .arg(args.at(1).toString(), args.at(0).toString()));
                return;
            }
            if (!property.isWritable()) {
                replyError(QDBusError::PropertyReadOnly,
                           QStringLiteral("Property '%1' is read-only").arg(args.at(1).toString()));
                return;
            }
            bool ok = false;
            const QVariant value =
                qDBusAdaptArgument(args.at(2).value<QDBusVariant>().variant(), property.metaType(), &ok);
            if (!ok || !property.write(target, value)) {
                replyError(QDBusError::InvalidArgs,
                           QStringLiteral("Invalid value for property '%1'").arg(args.at(1).toString()));
                return;
            }
            if (message.isReplyRequired())
                send(message.createReply());
            return;
        }
        if (member == QLatin1String("GetAll") && signature == "s") {
            const QString wanted = args.at(0).toString();
            QVariantMap values;
            for (const Candidate &c : candidatesOf(object, reg.options, reg.interface)) {
                if (!wanted.isEmpty() && wanted != c.interface)
                    continue;
                for (int i = c.cls->propertyOffset(); i < c.cls->propertyCount(); ++i) {
                    const QMetaProperty p = c.cls->property(i);
                    if (p.isReadable() && isExported(p, reg.options, c.adaptor) &&
                        QDBusMetaType::typeToSignature(p.metaType()))
                        values.insert(QString::fromLatin1(p.name()), p.read(c.target));
                }
            }
            if (message.isReplyRequired())
                send(message.createReply(QVariant::fromValue(values)));
            return;
        }
        replyError(QDBusError::UnknownMethod,
                   QStringLiteral("No such method '%1' in interface '%2'").arg(member, interface));
        return;
    }

    bool interfaceKnown = false;
    for (const Candidate &c : candidatesOf(object, reg.options, reg.interface)) {
        if (!interface.isEmpty() && interface != c.interface)
            continue;
        interfaceKnown = true;
        for (int i = c.cls->methodOffset(); i < c.cls->methodCount(); ++i) {
            const QMetaMethod m = c.cls->method(i);
            if (m.methodType() == QMetaMethod::Signal || member.toLatin1() != m.name() ||
                !isExported(m, reg.options, c.adaptor))
                continue;
            const QDBusMethodInfo info = QDBusMethodInfo::parse(m);
            if (!info.valid || info.inSignature != signature)
                continue;
            QDBusContextPrivate context{connection, message};
            QList<QVariant> outputs;
            QDBusError error;
            bool ok;
            {
                ContextScope scope(c.target, object, &context);
                ok = info.invoke(c.target, message, &outputs, &error);
            }
            if (!ok)
                replyError(error.type(), error.message());
            else if (message.isReplyRequired() && !message.isDelayedReply())
                send(message.createReply(outputs));
            return;
        }
    }
    if (!interface.isEmpty() && !interfaceKnown)
        replyError(QDBusError::UnknownInterface,
                   QStringLiteral("No such interface '%1' at object path '%2'").arg(interface, message.path()));
    else
        replyError(QDBusError::UnknownMethod,
                   QStringLiteral("No such method '%1' in interface '%2' at object path '%3' (signature '%4')")
                       .arg(member, interface, message.path(), QString::fromLatin1(signature)));
}

// ---- signal hooks --------------------------------------------------------------------------

void QDBusConnectionPrivate::addMatch(const QString &rule) {
    if (m_mode != BusMode)
        return;
    QDBusMessage m =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"), QStringLiteral("AddMatch"));
    m << rule;
    // Waiting makes sure that the bus routes the signals before connect() returns.
    if (QDBusConnectionManager::instance()->isIoThread())
        send(m);
    else
        call(m, QDBus::Block, -1);
}

void QDBusConnectionPrivate::removeMatch(const QString &rule) {
    if (m_mode != BusMode || !isConnected())
        return;
    QDBusMessage m =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"), QStringLiteral("RemoveMatch"));
    m << rule;
    send(m);
}

void QDBusConnectionPrivate::watchName(const QString &name) {
    if (m_mode != BusMode || name.isEmpty() || name.startsWith(QLatin1Char(':')) ||
        name == QLatin1String("org.freedesktop.DBus"))
        return;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        WatchedName &w = m_watched[name];
        if (w.refs++ > 0)
            return;
        m_matchRefs[matchRule(QStringLiteral("org.freedesktop.DBus"), QString(), QStringLiteral("org.freedesktop.DBus"),
                              QStringLiteral("NameOwnerChanged"), {name}, QString())]++;
    }
    addMatch(matchRule(QStringLiteral("org.freedesktop.DBus"), QString(), QStringLiteral("org.freedesktop.DBus"),
                       QStringLiteral("NameOwnerChanged"), {name}, QString()));
    QDBusMessage m =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    m << name;
    auto apply = [this, name](const QDBusMessage &reply) {
        const QString owner = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
                                  ? reply.arguments().first().toString()
                                  : QString();
        std::lock_guard<std::mutex> lock(m_tableMutex);
        const auto it = m_watched.find(name);
        if (it != m_watched.end() && !it->updated)
            it->owner = owner;
    };
    if (QDBusConnectionManager::instance()->isIoThread()) {
        const auto p = sendWithReply(m, -1);
        std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
        std::weak_ptr<QDBusPendingCallPrivate> call = p;
        p->onFinished([weak, call, name] {
            const auto self = weak.lock();
            const auto c = call.lock();
            if (!self || !c)
                return;
            const QDBusMessage reply = c->reply();
            std::lock_guard<std::mutex> lock(self->m_tableMutex);
            const auto it = self->m_watched.find(name);
            if (it != self->m_watched.end() && !it->updated && reply.type() == QDBusMessage::ReplyMessage &&
                !reply.arguments().isEmpty())
                it->owner = reply.arguments().first().toString();
        });
    } else {
        apply(call(m, QDBus::Block, -1));
    }
}

void QDBusConnectionPrivate::unwatchName(const QString &name) {
    if (m_mode != BusMode || name.isEmpty() || name.startsWith(QLatin1Char(':')) ||
        name == QLatin1String("org.freedesktop.DBus"))
        return;
    const QString rule =
        matchRule(QStringLiteral("org.freedesktop.DBus"), QString(), QStringLiteral("org.freedesktop.DBus"),
                  QStringLiteral("NameOwnerChanged"), {name}, QString());
    bool last = false;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        const auto it = m_watched.find(name);
        if (it == m_watched.end() || --it->refs > 0)
            return;
        m_watched.erase(it);
        last = --m_matchRefs[rule] <= 0;
        if (last)
            m_matchRefs.remove(rule);
    }
    if (last)
        removeMatch(rule);
}

uint QDBusConnectionPrivate::insertHook(Hook hook) {
    QObject *context = hook.receiver;
    std::weak_ptr<QDBusConnectionPrivate> weak = weak_from_this();
    bool firstRule = false;
    uint id;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        id = ++m_nextHookId;
        hook.id = id;
        firstRule = m_matchRefs[hook.rule]++ == 0;
    }
    hook.guard = QDBusTargetGuard::create(context, [weak, id] {
        if (auto self = weak.lock())
            self->removeHook(id);
    });
    const QString service = hook.service;
    const QString rule = hook.rule;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        m_hooks.push_back(std::make_shared<Hook>(std::move(hook)));
    }
    if (firstRule)
        addMatch(rule);
    watchName(service);
    return id;
}

void QDBusConnectionPrivate::removeHook(uint id) {
    std::shared_ptr<Hook> hook;
    bool lastRule = false;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it) {
            if ((*it)->id == id) {
                hook = *it;
                m_hooks.erase(it);
                break;
            }
        }
        if (!hook)
            return;
        *hook->active = false;
        lastRule = --m_matchRefs[hook->rule] <= 0;
        if (lastRule)
            m_matchRefs.remove(hook->rule);
    }
    if (lastRule)
        removeMatch(hook->rule);
    unwatchName(hook->service);
}

uint QDBusConnectionPrivate::addHook(const QString &service, const QString &path, const QString &interface,
                                     const QString &name, const QStringList &argumentMatch, const QString &argNamespace,
                                     QObject *context, std::function<void(const QDBusMessage &)> fn) {
    Hook hook;
    hook.service = service;
    hook.path = path;
    hook.interface = interface;
    hook.member = name;
    hook.argumentMatch = argumentMatch;
    hook.argNamespace = argNamespace;
    hook.rule = matchRule(service, path, interface, name, argumentMatch, argNamespace);
    hook.receiver = context;
    hook.function = std::move(fn);
    return insertHook(std::move(hook));
}

bool QDBusConnectionPrivate::connectSignal(const QString &service, const QString &path, const QString &interface,
                                           const QString &name, const QStringList &argumentMatch,
                                           const QString &signature, QObject *receiver, const char *slot) {
    if (!receiver || !slot)
        return false;
    if ((!service.isEmpty() && !QDBusUtil::isValidBusName(service)) ||
        (!path.isEmpty() && !QDBusUtil::isValidObjectPath(path)) ||
        (!interface.isEmpty() && !QDBusUtil::isValidInterfaceName(interface)) ||
        (!name.isEmpty() && !QDBusUtil::isValidMemberName(name)) ||
        (!signature.isEmpty() && !QDBusUtil::isValidSignature(signature.toLatin1())))
        return false;
    const QDBusMethodInfo info = QDBusMethodInfo::fromMember(receiver, slot);
    if (!info.valid || !info.outTypes.isEmpty()) {
        qWarning("QDBusConnection::connect: %s is not a slot that can receive D-Bus signals", slot);
        return false;
    }
    if (!signature.isEmpty() && !signature.toLatin1().startsWith(info.inSignature)) {
        qWarning("QDBusConnection::connect: the signature %s does not fit %s", qPrintable(signature), slot);
        return false;
    }
    Hook hook;
    hook.service = service;
    hook.path = path;
    hook.interface = interface;
    hook.member = name;
    hook.argumentMatch = argumentMatch;
    hook.signature = signature;
    hook.prefix = info.inSignature;
    hook.rule = matchRule(service, path, interface, name, argumentMatch, QString());
    hook.receiver = receiver;
    hook.slot = info;
    hook.slotSignature = info.method.methodSignature();
    insertHook(std::move(hook));
    return true;
}

bool QDBusConnectionPrivate::disconnectSignal(const QString &service, const QString &path, const QString &interface,
                                              const QString &name, const QStringList &argumentMatch,
                                              const QString &signature, QObject *receiver, const char *slot) {
    if (!receiver || !slot)
        return false;
    const QDBusMethodInfo info = QDBusMethodInfo::fromMember(receiver, slot);
    if (!info.valid)
        return false;
    const QByteArray slotSignature = info.method.methodSignature();
    uint id = 0;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (const auto &h : m_hooks) {
            if (!h->function && h->receiver == receiver && h->slotSignature == slotSignature && h->service == service &&
                h->path == path && h->interface == interface && h->member == name &&
                h->argumentMatch == argumentMatch && h->signature == signature) {
                id = h->id;
                break;
            }
        }
    }
    if (!id)
        return false;
    removeHook(id);
    return true;
}

void QDBusConnectionPrivate::deliverToHooks(const QDBusMessage &message) {
    const QList<QVariant> args = message.arguments();
    const QByteArray signature = message.signature().toLatin1();
    std::vector<std::shared_ptr<Hook>> matches;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        for (const auto &h : m_hooks) {
            if (!h->service.isEmpty() && m_mode == BusMode && h->service != message.service()) {
                const auto it = m_watched.find(h->service);
                if (it == m_watched.end() || it->owner.isEmpty() || it->owner != message.service())
                    continue;
            }
            if ((!h->path.isEmpty() && h->path != message.path()) ||
                (!h->interface.isEmpty() && h->interface != message.interface()) ||
                (!h->member.isEmpty() && h->member != message.member()))
                continue;
            bool ok = true;
            for (qsizetype i = 0; ok && i < h->argumentMatch.size(); ++i)
                if (!h->argumentMatch.at(i).isEmpty())
                    ok = i < args.size() && args.at(i).metaType() == QMetaType::fromType<QString>() &&
                         args.at(i).toString() == h->argumentMatch.at(i);
            if (ok && !h->argNamespace.isEmpty()) {
                const QString arg0 = args.isEmpty() ? QString() : args.first().toString();
                ok = arg0 == h->argNamespace || arg0.startsWith(h->argNamespace + QLatin1Char('.'));
            }
            if (!ok)
                continue;
            if (!h->function &&
                (!signature.startsWith(h->prefix) || (!h->signature.isEmpty() && signature != h->signature.toLatin1())))
                continue;
            matches.push_back(h);
        }
    }
    for (const auto &h : matches) {
        h->guard->post([h, message] {
            if (!*h->active)
                return;
            if (h->function) {
                h->function(message);
                return;
            }
            QObject *receiver = h->guard->target();
            if (!receiver)
                return;
            QDBusError error;
            if (!h->slot.invoke(receiver, message, nullptr, &error))
                qWarning("QDBusConnection: cannot deliver %s to %s: %s", qPrintable(message.member()),
                         h->slotSignature.constData(), qPrintable(error.message()));
        });
    }
}
