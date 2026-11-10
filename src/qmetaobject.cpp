// MIT License - see LICENSE

/*!
    \page meta-object-system.html
    \title Meta-Object System
    \brief How Nt describes classes at run time: introspection, properties and invoking by name.

    A class that contains \c Q_OBJECT (or \c Q_GADGET) and goes through the
    \c ntmoc tool has a QMetaObject. \c ntmoc reads the class declaration and
    records:

    \list
    \li the class name and the base class;
    \li signals, slots and functions marked \c Q_INVOKABLE, with their
        parameter types and names, and their default arguments;
    \li constructors marked \c Q_INVOKABLE;
    \li properties declared with \c Q_PROPERTY();
    \li enumerations declared with \c Q_ENUM() and \c Q_FLAG();
    \li class information declared with \c Q_CLASSINFO().
    \endlist

    \section1 Using it

    \code
    class Counter : public QObject {
        Q_OBJECT
        Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
        Q_CLASSINFO("Version", "1.0")
    public:
        enum Mode { Up, Down };
        Q_ENUM(Mode)
        int value() const { return m_value; }
        Q_INVOKABLE void reset() { setValue(0); }
    public slots:
        void setValue(int v);
    signals:
        void valueChanged(int value);
    private:
        int m_value = 0;
    };

    Counter c;
    c.setProperty("value", 5);                         // via the property system
    QMetaObject::invokeMethod(&c, "reset");            // by name
    c.metaObject()->className();                       // "Counter"
    QObject::connect(&c, SIGNAL(valueChanged(int)), &other, SLOT(setValue(int)));
    \endcode

    \section1 Properties

    \c QObject::property() and \c QObject::setProperty() read and write
    properties by name, converting values through QVariant. A name that is not
    a declared property creates a \e dynamic property on the object; its
    changes are reported with a QDynamicPropertyChangeEvent.

    \section1 Value types

    QMetaType describes the types that properties, method arguments and
    QVariant hold. Types that Nt does not know need
    \c Q_DECLARE_METATYPE() or \c qRegisterMetaType() before they can be queued,
    stored in a QVariant or looked up by name.

    \section1 Limits

    The meta-object of a class exists only if \c ntmoc processed its header.
    Code that does not use \c ntmoc, such as a class that only declares signals
    with \c emitSignal() bodies, keeps working but has no QMetaObject of its
    own: QObject::metaObject() returns that of its nearest base class that has
    one. \c ntmoc is a text scanner and does not evaluate the preprocessor, so
    declarations produced by macros are not seen.

    \sa QMetaObject, QMetaType, QVariant, {Signals and Slots}
*/

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

#include "ntnormalize_p.h"
#include "qobject_p.h"
#include "threaddata_p.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

using QtPrivate::MetaMethodData;

namespace {

constexpr int kMaxArgs = 10;

struct MetaRegistry {
    std::mutex mutex;
    std::unordered_map<std::type_index, const QMetaObject *> byType;
    std::vector<std::pair<const QMetaObject *, bool (*)(const QObject *)>> instances;
};

// Never destroyed: objects in static storage may ask for their meta-object while the program exits.
MetaRegistry &metaRegistry() {
    static MetaRegistry *r = new MetaRegistry; // NOLINT(cppcoreguidelines-owning-memory)
    return *r;
}

int localCount(unsigned n) { return static_cast<int>(n); }

int depthOf(const QMetaObject *m) {
    int n = 0;
    for (; m; m = m->d.superdata)
        ++n;
    return n;
}

std::string signatureOf(const MetaMethodData &m) {
    std::string s = m.name;
    s += '(';
    for (unsigned i = 0; i < m.paramCount; ++i) {
        if (i)
            s += ',';
        s += m.paramTypes[i];
    }
    return s + ')';
}

// Method types as bits, for searching.
constexpr unsigned kBitMethod = 1u << QMetaMethod::Method;
constexpr unsigned kBitSignal = 1u << QMetaMethod::Signal;
constexpr unsigned kBitSlot = 1u << QMetaMethod::Slot;

int indexOfMethodKind(const QMetaObject *mo, const char *method, unsigned kinds) {
    if (!mo || !method)
        return -1;
    const std::string sig = NtMeta::normalizeSignature(method);
    for (const QMetaObject *m = mo; m; m = m->d.superdata) {
        const auto &data = *m->d.data;
        for (unsigned i = data.methodCount; i-- > 0;) {
            const MetaMethodData &md = data.methods[i];
            if (((1u << md.type) & kinds) && signatureOf(md) == sig)
                return m->methodOffset() + static_cast<int>(i);
        }
    }
    return -1;
}

// Resolves Qt::AutoConnection by comparing the thread of `object` with the calling thread, and
// calls a blocking connection within one thread directly.
Qt::ConnectionType effectiveConnectionType(const QObject *object, Qt::ConnectionType type) {
    const bool crossThread = QtPrivate::objectThreadData(object) != QtPrivate::ThreadData::current();
    if (type == Qt::AutoConnection)
        return crossThread ? Qt::QueuedConnection : Qt::DirectConnection;
    if (type == Qt::BlockingQueuedConnection && !crossThread)
        return Qt::DirectConnection;
    return type;
}

void collectDescendants(QObject *o, QObjectList &out) {
    for (QObject *c : o->children()) {
        out.append(c);
        collectDescendants(c, out);
    }
}

QByteArray toBytes(const std::string &s) { return QByteArray(s.c_str(), static_cast<qsizetype>(s.size())); }

// Whether the type called `given` is the declared type `declared`, whose meta-type is `declaredType`.
// A type may be spelled with or without the scope of its enclosing class.
bool sameTypeName(const char *given, const char *declared, QMetaType declaredType) {
    if (!given)
        return false;
    const std::string name = NtMeta::normalizeType(given);
    if (name == declared)
        return true;
    const QMetaType a = QMetaType::fromName(QByteArrayView(given));
    if (a.isValid() && declaredType.isValid() && a == declaredType)
        return true;
    if (const char *full = declaredType.name()) {
        const std::string f = full;
        if (f.size() > name.size() + 2 && f.compare(f.size() - name.size(), name.size(), name) == 0 &&
            f.compare(f.size() - name.size() - 2, 2, "::") == 0)
            return true;
    }
    return false;
}

} // namespace

namespace QtPrivate {

const QMetaObject *registerMetaObject(const std::type_info &type, const QMetaObject *self, const QMetaObject *super,
                                      bool (*isInstance)(const QObject *)) {
    MetaRegistry &r = metaRegistry();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.byType.insert_or_assign(std::type_index(type), self);
    if (isInstance)
        r.instances.emplace_back(self, isInstance);
    return super;
}

const QMetaObject *findMetaObject(const std::type_info &type) {
    MetaRegistry &r = metaRegistry();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto it = r.byType.find(std::type_index(type));
    return it == r.byType.end() ? nullptr : it->second;
}

const QMetaObject *findMetaObjectOf(const QObject *object) {
    MetaRegistry &r = metaRegistry();
    {
        std::lock_guard<std::mutex> lock(r.mutex);
        const auto it = r.byType.find(std::type_index(typeid(*object)));
        if (it != r.byType.end())
            return it->second;
    }
    // A class that ntmoc did not process: use the registered class it derives from most deeply.
    std::vector<std::pair<const QMetaObject *, bool (*)(const QObject *)>> candidates;
    {
        std::lock_guard<std::mutex> lock(r.mutex);
        candidates = r.instances;
    }
    const QMetaObject *best = nullptr;
    int bestDepth = 0;
    for (const auto &c : candidates) {
        const int depth = depthOf(c.first);
        if (depth > bestDepth && c.second(object)) {
            best = c.first;
            bestDepth = depth;
        }
    }
    if (!best)
        best = &QObject::staticMetaObject;
    std::lock_guard<std::mutex> lock(r.mutex);
    r.byType.emplace(std::type_index(typeid(*object)), best);
    return best;
}

} // namespace QtPrivate

// QMetaObject

/*!
    Returns the name of the class this meta-object describes, including the names of the
    namespaces and classes that enclose it, such as \c{"outer::Inner"}.

    \sa superClass()
*/
const char *QMetaObject::className() const { return d.data->className; }

/*!
    Returns the meta-object of the base class, or \c nullptr if there is none.

    \sa inherits()
*/
const QMetaObject *QMetaObject::superClass() const { return d.superdata; }

/*!
    Returns \c true if the class described by this meta-object is \a metaObject or inherits from
    it; otherwise returns \c false.

    \sa cast(), superClass()
*/
bool QMetaObject::inherits(const QMetaObject *metaObject) const noexcept {
    for (const QMetaObject *m = this; m; m = m->d.superdata)
        if (m == metaObject)
            return true;
    return false;
}

/*!
    Returns \a obj if it is an instance of the class this meta-object describes or of a class
    derived from it; otherwise returns \c nullptr.

    \sa inherits()
*/
QObject *QMetaObject::cast(QObject *obj) const { return obj && obj->metaObject()->inherits(this) ? obj : nullptr; }

/*!
    \overload
    Returns \a obj if it is an instance of this class or of a class derived from it; otherwise
    returns \c nullptr.
*/
const QObject *QMetaObject::cast(const QObject *obj) const {
    return obj && obj->metaObject()->inherits(this) ? obj : nullptr;
}

/*!
    Returns the index of the first method of this class: the number of methods of its base
    classes.

    \sa methodCount(), method()
*/
int QMetaObject::methodOffset() const {
    int n = 0;
    for (const QMetaObject *m = d.superdata; m; m = m->d.superdata)
        n += localCount(m->d.data->methodCount);
    return n;
}

/*!
    Returns the index of the first enumerator of this class.

    \sa enumeratorCount(), enumerator()
*/
int QMetaObject::enumeratorOffset() const {
    int n = 0;
    for (const QMetaObject *m = d.superdata; m; m = m->d.superdata)
        n += localCount(m->d.data->enumCount);
    return n;
}

/*!
    Returns the index of the first property of this class.

    \sa propertyCount(), property()
*/
int QMetaObject::propertyOffset() const {
    int n = 0;
    for (const QMetaObject *m = d.superdata; m; m = m->d.superdata)
        n += localCount(m->d.data->propertyCount);
    return n;
}

/*!
    Returns the index of the first class information entry of this class.

    \sa classInfoCount(), classInfo()
*/
int QMetaObject::classInfoOffset() const {
    int n = 0;
    for (const QMetaObject *m = d.superdata; m; m = m->d.superdata)
        n += localCount(m->d.data->classInfoCount);
    return n;
}

/*!
    Returns the number of invokable constructors of this class. Constructors are not inherited.

    \sa constructor(), newInstance()
*/
int QMetaObject::constructorCount() const { return localCount(d.data->constructorCount); }

/*!
    Returns the number of methods of this class, including those of its base classes.

    \sa methodOffset(), method()
*/
int QMetaObject::methodCount() const { return methodOffset() + localCount(d.data->methodCount); }

/*!
    Returns the number of enumerators of this class, including those of its base classes.

    \sa enumeratorOffset(), enumerator()
*/
int QMetaObject::enumeratorCount() const { return enumeratorOffset() + localCount(d.data->enumCount); }

/*!
    Returns the number of properties of this class, including those of its base classes.

    \sa propertyOffset(), property()
*/
int QMetaObject::propertyCount() const { return propertyOffset() + localCount(d.data->propertyCount); }

/*!
    Returns the number of class information entries of this class, including those of its base
    classes.

    \sa classInfoOffset(), classInfo()
*/
int QMetaObject::classInfoCount() const { return classInfoOffset() + localCount(d.data->classInfoCount); }

/*!
    Returns the index of the constructor with the signature \a constructor, such as
    \c{"Counter(QObject*)"}, or -1 if there is none.

    \sa constructor(), newInstance()
*/
int QMetaObject::indexOfConstructor(const char *constructor) const {
    if (!constructor)
        return -1;
    const std::string sig = NtMeta::normalizeSignature(constructor);
    for (unsigned i = 0; i < d.data->constructorCount; ++i)
        if (signatureOf(d.data->constructors[i]) == sig)
            return static_cast<int>(i);
    return -1;
}

/*!
    Returns the index of the signal, slot or invokable method with the signature \a method, such
    as \c{"setValue(int)"}, or -1 if there is none. Base classes are searched too.

    \sa indexOfSignal(), indexOfSlot(), method()
*/
int QMetaObject::indexOfMethod(const char *method) const {
    return indexOfMethodKind(this, method, kBitMethod | kBitSignal | kBitSlot);
}

/*!
    Returns the index of the signal with the signature \a signal, such as
    \c{"valueChanged(int)"}, or -1 if there is none.

    \sa indexOfMethod(), indexOfSlot()
*/
int QMetaObject::indexOfSignal(const char *signal) const { return indexOfMethodKind(this, signal, kBitSignal); }

/*!
    Returns the index of the slot with the signature \a slot, such as \c{"setValue(int)"}, or -1
    if there is none.

    \sa indexOfMethod(), indexOfSignal()
*/
int QMetaObject::indexOfSlot(const char *slot) const { return indexOfMethodKind(this, slot, kBitSlot); }

/*!
    Returns the index of the enumerator called \a name, or -1 if there is none. The name is
    that of the enumeration, without its scope.

    \sa enumerator()
*/
int QMetaObject::indexOfEnumerator(const char *name) const {
    if (!name)
        return -1;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const auto &data = *m->d.data;
        for (unsigned i = 0; i < data.enumCount; ++i)
            if (std::strcmp(data.enums[i].name, name) == 0)
                return m->enumeratorOffset() + static_cast<int>(i);
    }
    return -1;
}

/*!
    Returns the index of the property called \a name, or -1 if there is none.

    \sa property()
*/
int QMetaObject::indexOfProperty(const char *name) const {
    if (!name)
        return -1;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const auto &data = *m->d.data;
        for (unsigned i = 0; i < data.propertyCount; ++i)
            if (std::strcmp(data.properties[i].name, name) == 0)
                return m->propertyOffset() + static_cast<int>(i);
    }
    return -1;
}

/*!
    Returns the index of the class information entry called \a name, or -1 if there is none.

    \sa classInfo()
*/
int QMetaObject::indexOfClassInfo(const char *name) const {
    if (!name)
        return -1;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const auto &data = *m->d.data;
        for (unsigned i = 0; i < data.classInfoCount; ++i)
            if (std::strcmp(data.classInfos[i].name, name) == 0)
                return m->classInfoOffset() + static_cast<int>(i);
    }
    return -1;
}

/*!
    Returns the meta-data of the constructor at \a index, or an invalid QMetaMethod if \a index
    is out of range.

    \sa constructorCount(), indexOfConstructor()
*/
QMetaMethod QMetaObject::constructor(int index) const {
    QMetaMethod m;
    if (index >= 0 && index < constructorCount()) {
        m.mobj = this;
        m.index = index;
        m.ctor = true;
    }
    return m;
}

/*!
    Returns the meta-data of the method at \a index, counting the methods of the base classes
    first, or an invalid QMetaMethod if \a index is out of range.

    \sa methodCount(), indexOfMethod()
*/
QMetaMethod QMetaObject::method(int index) const {
    QMetaMethod r;
    if (index < 0)
        return r;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const int off = m->methodOffset();
        if (index >= off) {
            if (index - off < localCount(m->d.data->methodCount)) {
                r.mobj = m;
                r.index = index - off;
            }
            return r;
        }
    }
    return r;
}

/*!
    Returns the meta-data of the enumerator at \a index, counting the enumerators of the base
    classes first, or an invalid QMetaEnum if \a index is out of range.

    \sa enumeratorCount(), indexOfEnumerator()
*/
QMetaEnum QMetaObject::enumerator(int index) const {
    QMetaEnum r;
    if (index < 0)
        return r;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const int off = m->enumeratorOffset();
        if (index >= off) {
            if (index - off < localCount(m->d.data->enumCount)) {
                r.mobj = m;
                r.index = index - off;
            }
            return r;
        }
    }
    return r;
}

/*!
    Returns the meta-data of the property at \a index, counting the properties of the base
    classes first, or an invalid QMetaProperty if \a index is out of range.

    \sa propertyCount(), indexOfProperty()
*/
QMetaProperty QMetaObject::property(int index) const {
    QMetaProperty r;
    if (index < 0)
        return r;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const int off = m->propertyOffset();
        if (index >= off) {
            if (index - off < localCount(m->d.data->propertyCount)) {
                r.mobj = m;
                r.index = index - off;
            }
            return r;
        }
    }
    return r;
}

/*!
    Returns the class information entry at \a index, counting those of the base classes first,
    or an invalid QMetaClassInfo if \a index is out of range.

    \sa classInfoCount(), indexOfClassInfo()
*/
QMetaClassInfo QMetaObject::classInfo(int index) const {
    QMetaClassInfo r;
    if (index < 0)
        return r;
    for (const QMetaObject *m = this; m; m = m->d.superdata) {
        const int off = m->classInfoOffset();
        if (index >= off) {
            if (index - off < localCount(m->d.data->classInfoCount)) {
                r.mobj = m;
                r.index = index - off;
            }
            return r;
        }
    }
    return r;
}

/*!
    Returns the property that has the \c USER attribute set, or an invalid QMetaProperty if
    there is none.

    \sa QMetaProperty::isUser()
*/
QMetaProperty QMetaObject::userProperty() const {
    for (int i = propertyCount() - 1; i >= 0; --i) {
        const QMetaProperty p = property(i);
        if (p.isUser())
            return p;
    }
    return QMetaProperty();
}

/*!
    Returns \c true if the arguments of the slot or signal \a method are compatible with those
    of \a signal: \a method may have fewer arguments, and each of its types must equal the type
    of the signal argument in the same position. Both are given as signatures without the
    \c SIGNAL() or \c SLOT() prefix.

    \sa normalizedSignature()
*/
bool QMetaObject::checkConnectArgs(const char *signal, const char *method) {
    if (!signal || !method)
        return false;
    const std::string s = NtMeta::normalizeSignature(signal);
    const std::string m = NtMeta::normalizeSignature(method);
    const auto args = [](const std::string &sig) {
        const std::size_t open = sig.find('(');
        return NtMeta::splitTopLevel(sig.substr(open + 1, sig.size() - open - 2));
    };
    const std::vector<std::string> sa = args(s);
    const std::vector<std::string> ma = args(m);
    if (ma.size() > sa.size())
        return false;
    for (std::size_t i = 0; i < ma.size(); ++i)
        if (ma[i] != sa[i])
            return false;
    return true;
}

/*!
    \overload
    Returns \c true if the arguments of \a method are compatible with those of \a signal.
*/
bool QMetaObject::checkConnectArgs(const QMetaMethod &signal, const QMetaMethod &method) {
    if (!signal.isValid() || !method.isValid())
        return false;
    return checkConnectArgs(signal.methodSignature().constData(), method.methodSignature().constData());
}

/*!
    Returns \a method as a normalized signature: no whitespace except in type names that need
    it, \c const and \c & removed from by-value and const-reference parameters, and the spelled-out
    names of fundamental types abbreviated. For example,
    \c{"setValue( const QString & , unsigned int )"} becomes \c{"setValue(QString,uint)"}.

    \sa normalizedType()
*/
QByteArray QMetaObject::normalizedSignature(const char *method) {
    return toBytes(NtMeta::normalizeSignature(method ? method : ""));
}

/*!
    Returns \a type as a normalized type name; see normalizedSignature().
*/
QByteArray QMetaObject::normalizedType(const char *type) { return toBytes(NtMeta::normalizeType(type ? type : "")); }

/*!
    Creates a new instance of the class with a constructor that takes the arguments \a val0,
    \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9, and returns
    it, or \c nullptr if there is no such constructor. Only constructors
    declared with \c Q_INVOKABLE are known.

    \sa QMetaMethod::newInstance(), constructor()
*/
QObject *QMetaObject::newInstance(QGenericArgument val0, QGenericArgument val1, QGenericArgument val2,
                                  QGenericArgument val3, QGenericArgument val4, QGenericArgument val5,
                                  QGenericArgument val6, QGenericArgument val7, QGenericArgument val8,
                                  QGenericArgument val9) const {
    const QGenericArgument *vals[] = {&val0, &val1, &val2, &val3, &val4, &val5, &val6, &val7, &val8, &val9};
    std::string sig = std::string(d.data->className);
    const std::size_t colon = sig.rfind("::");
    if (colon != std::string::npos)
        sig.erase(0, colon + 2);
    sig += '(';
    int n = 0;
    for (; n < kMaxArgs && vals[n]->name(); ++n) {
        if (n)
            sig += ',';
        sig += NtMeta::normalizeType(vals[n]->name());
    }
    sig += ')';
    const int idx = indexOfConstructor(sig.c_str());
    if (idx < 0)
        return nullptr;
    return constructor(idx).newInstance(val0, val1, val2, val3, val4, val5, val6, val7, val8, val9);
}

/*!
    Connects the signal with the index \a signal_index of \a sender to the method with the index
    \a method_index of \a receiver, and returns \c true on success. \a type is a
    \c Qt::ConnectionType and \a types is ignored.

    \sa QObject::connect(), disconnect()
*/
bool QMetaObject::connect(const QObject *sender, int signal_index, const QObject *receiver, int method_index, int type,
                          int *types) {
    Q_UNUSED(types)
    if (!sender || !receiver)
        return false;
    const QMetaMethod s = sender->metaObject()->method(signal_index);
    const QMetaMethod m = receiver->metaObject()->method(method_index);
    return static_cast<bool>(QObject::connect(sender, s, receiver, m, static_cast<Qt::ConnectionType>(type)));
}

/*!
    Removes the connections from the signal with the index \a signal_index of \a sender to the
    method with the index \a method_index of \a receiver. A negative \a signal_index or
    \a method_index matches any signal or any method. Returns \c true if a connection was
    removed.

    \sa connect()
*/
bool QMetaObject::disconnect(const QObject *sender, int signal_index, const QObject *receiver, int method_index) {
    if (!sender)
        return false;
    const QMetaMethod s = signal_index < 0 ? QMetaMethod() : sender->metaObject()->method(signal_index);
    const QMetaMethod m = method_index < 0 || !receiver ? QMetaMethod() : receiver->metaObject()->method(method_index);
    if ((signal_index >= 0 && !s.isValid()) || (method_index >= 0 && receiver && !m.isValid()))
        return false;
    if (!s.isValid() && !m.isValid())
        return sender->disconnect(receiver);
    if (!s.isValid()) {
        const std::string sig = m.methodSignature().constData();
        return QtPrivate::disconnectConnections(sender, nullptr, receiver, &sig);
    }
    return QObject::disconnect(sender, s, receiver, m);
}

/*!
    Connects signals to slots by name, for each slot of \a o that is named
    \c{on_<object name>_<signal name>(...)}. The object name is that of \a o or of one of its
    descendants, and the signal is a signal of that object whose arguments are compatible with
    the slot. A slot that finds no signal prints a warning.

    \sa QObject::setObjectName()
*/
void QMetaObject::connectSlotsByName(QObject *o) {
    if (!o)
        return;
    const QMetaObject *mo = o->metaObject();
    QObjectList children;
    collectDescendants(o, children);
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod slot = mo->method(i);
        if (slot.methodType() != QMetaMethod::Slot)
            continue;
        const std::string name = slot.name().constData();
        if (name.compare(0, 3, "on_") != 0)
            continue;
        const std::size_t sep = name.find('_', 3);
        bool connectedAny = false;
        // Object names can contain underscores, so try each possible split.
        for (std::size_t pos = sep; pos != std::string::npos; pos = name.find('_', pos + 1)) {
            const std::string objectName = name.substr(3, pos - 3);
            const std::string signalName = name.substr(pos + 1);
            if (objectName.empty() || signalName.empty())
                continue;
            QObjectList candidates;
            if (o->objectName() == QString::fromUtf8(objectName.c_str()))
                candidates.append(o);
            for (QObject *c : children)
                if (c->objectName() == QString::fromUtf8(objectName.c_str()))
                    candidates.append(c);
            for (QObject *target : candidates) {
                const QMetaObject *tmo = target->metaObject();
                for (int j = 0; j < tmo->methodCount(); ++j) {
                    const QMetaMethod sig = tmo->method(j);
                    if (sig.methodType() != QMetaMethod::Signal || sig.name().toStdString() != signalName)
                        continue;
                    if (!QMetaObject::checkConnectArgs(sig, slot))
                        continue;
                    if (QObject::connect(target, sig, o, slot))
                        connectedAny = true;
                    break;
                }
            }
        }
        if (!connectedAny)
            qWarning("QMetaObject::connectSlotsByName: No matching signal for %s", slot.methodSignature().constData());
    }
}

/*!
    Emits the signal with the index \a signal_index of \a sender, with the arguments at
    \a argv, where \c{argv[1]} is the first one. \c{argv[0]} is reserved for a return value and is unused for
    signals. This is the call that a signal's generated body makes in Qt; Nt's \c ntmoc output
    does not use it, but code that does works.

    \sa QObject::connect()
*/
void QMetaObject::activate(QObject *sender, int signal_index, void **argv) {
    if (!sender)
        return;
    const QMetaMethod m = sender->metaObject()->method(signal_index);
    if (m.methodType() != QMetaMethod::Signal)
        return;
    const QtPrivate::MetaMethodData &md = m.data();
    if (!md.signalId)
        return;
    std::vector<QMetaType> types;
    types.reserve(static_cast<std::size_t>(m.parameterCount()));
    for (int i = 0; i < m.parameterCount(); ++i)
        types.push_back(m.parameterMetaType(i));
    QtPrivate::ArgCloner clone = [types](void *const *args) {
        QtPrivate::QueuedArgs q;
        auto storage = std::make_shared<std::vector<std::pair<QMetaType, void *>>>();
        for (std::size_t i = 0; i < types.size(); ++i) {
            void *copy = types[i].create(args[i]);
            storage->emplace_back(types[i], copy);
            q.argv.push_back(copy);
        }
        q.storage = std::shared_ptr<void>(storage.get(), [storage](void *) {
            for (auto &e : *storage)
                e.first.destroy(e.second);
        });
        return q;
    };
    QObject::activateImpl(sender, md.signalId(), argv ? argv + 1 : nullptr, clone);
}

/*!
    \overload
    Emits the signal with the index \a local_signal_index in \a mo, a meta-object in the chain
    of the meta-objects of \a sender, with the arguments \a argv.
*/
void QMetaObject::activate(QObject *sender, const QMetaObject *mo, int local_signal_index, void **argv) {
    if (!mo)
        return;
    activate(sender, mo->methodOffset() + local_signal_index, argv);
}

/*!
    Invokes the signal, slot or invokable method \a member of \a obj and returns \c true if it
    could be invoked. \a member is the name of the method without its parameters. The
    arguments \a val0, \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and
    \a val9, made with \c Q_ARG(), select the overload by their types, and \a ret, made with
    \c Q_RETURN_ARG(), receives the return value.

    \a type is how the call is made: with \c Qt::DirectConnection the method runs before the
    function returns, and with \c Qt::QueuedConnection it runs from the event loop of the thread
    of \a obj, with copies of the arguments. \c Qt::AutoConnection calls directly when \a obj
    lives in the calling thread and queues otherwise. \c Qt::BlockingQueuedConnection queues the
    call to another thread and waits for it, so it can return a value. A queued call cannot
    return a value, and its argument types must be registered with QMetaType.

    \code
    QString result;
    QMetaObject::invokeMethod(obj, "describe", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, result), Q_ARG(int, 3));
    \endcode

    The function prints a warning and returns \c false if \a obj has no such method or the
    argument types do not match.

    \sa QMetaMethod::invoke()
*/
bool QMetaObject::invokeMethod(QObject *obj, const char *member, Qt::ConnectionType type, QGenericReturnArgument ret,
                               QGenericArgument val0, QGenericArgument val1, QGenericArgument val2,
                               QGenericArgument val3, QGenericArgument val4, QGenericArgument val5,
                               QGenericArgument val6, QGenericArgument val7, QGenericArgument val8,
                               QGenericArgument val9) {
    if (!obj || !member)
        return false;
    const QGenericArgument *vals[] = {&val0, &val1, &val2, &val3, &val4, &val5, &val6, &val7, &val8, &val9};
    const QMetaObject *mo = obj->metaObject();
    std::string sig = member;
    sig += '(';
    int n = 0;
    for (; n < kMaxArgs && vals[n]->name(); ++n) {
        if (n)
            sig += ',';
        sig += NtMeta::normalizeType(vals[n]->name());
    }
    sig += ')';
    int idx = mo->indexOfMethod(sig.c_str());
    if (idx < 0) {
        // The argument may be spelled with another name for the same type.
        for (int i = mo->methodCount() - 1; i >= 0 && idx < 0; --i) {
            const QMetaMethod m = mo->method(i);
            if (m.parameterCount() != n || m.name() != member)
                continue;
            bool same = true;
            for (int j = 0; j < n && same; ++j)
                same = sameTypeName(vals[j]->name(), m.parameterTypes().at(j).constData(), m.parameterMetaType(j));
            if (same)
                idx = i;
        }
    }
    if (idx < 0) {
        qWarning("QMetaObject::invokeMethod: No such method %s::%s", mo->className(), sig.c_str());
        return false;
    }
    return mo->method(idx).invoke(obj, type, ret, val0, val1, val2, val3, val4, val5, val6, val7, val8, val9);
}

Qt::ConnectionType QMetaObject::resolveConnectionType(const QObject *context, Qt::ConnectionType type) {
    return effectiveConnectionType(context, type);
}

bool QMetaObject::invokeFunctor(QObject *context, std::function<void()> function, Qt::ConnectionType type) {
    if (!context)
        return false;
    type = effectiveConnectionType(context, type);
    if (type == Qt::QueuedConnection) {
        QCoreApplication::postEvent(context, new QMetaCallEvent(std::move(function)));
        return true;
    }
    if (type == Qt::BlockingQueuedConnection) {
        auto latch = std::make_shared<QtPrivate::BlockingLatch>();
        QCoreApplication::postEvent(context, new QMetaCallEvent(std::move(function), latch));
        latch->wait();
        return true;
    }
    function();
    return true;
}

// QMetaMethod

const MetaMethodData &QMetaMethod::data() const {
    return ctor ? mobj->d.data->constructors[index] : mobj->d.data->methods[index];
}

/*!
    Returns the signature of the method, such as \c{"setValue(int)"}. Types are normalized.

    \sa name(), parameterTypes()
*/
QByteArray QMetaMethod::methodSignature() const {
    if (!isValid())
        return QByteArray();
    return toBytes(signatureOf(data()));
}

/*!
    Returns the name of the method without its parameters.

    \sa methodSignature()
*/
QByteArray QMetaMethod::name() const { return isValid() ? QByteArray(data().name) : QByteArray(); }

/*!
    Returns the name of the return type of the method, \c{"void"} for a method that returns
    nothing, or an empty string for a constructor.

    \sa returnType()
*/
const char *QMetaMethod::typeName() const { return isValid() && !ctor ? data().returnType : ""; }

/*!
    Returns the \c QMetaType id of the return type, or \c QMetaType::UnknownType if it is not
    known.

    \sa returnMetaType()
*/
int QMetaMethod::returnType() const { return returnMetaType().id(); }

/*!
    Returns the QMetaType of the return type.

    \sa returnType()
*/
QMetaType QMetaMethod::returnMetaType() const {
    if (!isValid() || ctor)
        return QMetaType();
    QMetaType t;
    if (mobj->d.static_metacall) {
        int arg = -1;
        void *a[] = {&t, &arg};
        mobj->d.static_metacall(nullptr, QMetaObject::RegisterMethodArgumentMetaType, index, a);
    }
    return t.isValid() ? t : QMetaType::fromName(QByteArrayView(data().returnType));
}

/*!
    Returns the number of parameters of the method.

    \sa parameterTypes(), parameterNames()
*/
int QMetaMethod::parameterCount() const { return isValid() ? data().paramCount : 0; }

/*!
    Returns the \c QMetaType id of the parameter at \a param, or \c QMetaType::UnknownType if
    \a param is out of range or the type is not known.

    \sa parameterMetaType()
*/
int QMetaMethod::parameterType(int param) const { return parameterMetaType(param).id(); }

/*!
    Returns the QMetaType of the parameter at \a param, or an invalid QMetaType if \a param is
    out of range.

    \sa parameterType()
*/
QMetaType QMetaMethod::parameterMetaType(int param) const {
    if (!isValid() || param < 0 || param >= parameterCount())
        return QMetaType();
    QMetaType t;
    if (!ctor && mobj->d.static_metacall) {
        int arg = param;
        void *a[] = {&t, &arg};
        mobj->d.static_metacall(nullptr, QMetaObject::RegisterMethodArgumentMetaType, index, a);
    }
    return t.isValid() ? t : QMetaType::fromName(QByteArrayView(data().paramTypes[param]));
}

/*!
    Stores the \c QMetaType ids of the parameters in \a types, which must have room for
    parameterCount() entries.

    \sa parameterType()
*/
void QMetaMethod::getParameterTypes(int *types) const {
    for (int i = 0; i < parameterCount(); ++i)
        types[i] = parameterType(i);
}

/*!
    Returns the names of the parameter types, such as \c{"QString"}, in order.

    \sa parameterNames()
*/
QList<QByteArray> QMetaMethod::parameterTypes() const {
    QList<QByteArray> list;
    for (int i = 0; i < parameterCount(); ++i)
        list.append(QByteArray(data().paramTypes[i]));
    return list;
}

/*!
    Returns the names of the parameters in order. A parameter that is declared without a name
    has an empty name.

    \sa parameterTypes()
*/
QList<QByteArray> QMetaMethod::parameterNames() const {
    QList<QByteArray> list;
    for (int i = 0; i < parameterCount(); ++i)
        list.append(QByteArray(data().paramNames[i]));
    return list;
}

/*!
    Returns the tag of the method, which is an empty string unless a tag was set with moc.
*/
const char *QMetaMethod::tag() const { return isValid() && data().tag ? data().tag : ""; }

/*!
    Returns the attributes of the method, a combination of QMetaMethod::Attributes values.
*/
int QMetaMethod::attributes() const {
    if (!isValid())
        return 0;
    const unsigned flags = data().flags;
    return ((flags & 1) ? Cloned : 0) | ((flags & 2) ? Scriptable : 0);
}

/*!
    Returns the access level of the method. Signals are public.

    \sa methodType()
*/
QMetaMethod::Access QMetaMethod::access() const { return isValid() ? static_cast<Access>(data().access) : Public; }

/*!
    Returns the kind of the method.

    \sa access()
*/
QMetaMethod::MethodType QMetaMethod::methodType() const {
    return isValid() ? static_cast<MethodType>(data().type) : Method;
}

/*!
    Returns the index of the method in its class, counting the methods of the base classes
    first. For a constructor, returns its index among the constructors.

    \sa relativeMethodIndex(), QMetaObject::method()
*/
int QMetaMethod::methodIndex() const {
    if (!isValid())
        return -1;
    return ctor ? index : mobj->methodOffset() + index;
}

/*!
    Returns the index of the method among the methods of its own class, not counting those of
    the base classes.

    \sa methodIndex()
*/
int QMetaMethod::relativeMethodIndex() const { return isValid() ? index : -1; }

/*!
    Returns the revision of the method, which is 0 unless it is declared with \c Q_REVISION().
*/
int QMetaMethod::revision() const { return isValid() ? data().revision : 0; }

/*!
    Returns the meta-object of the class that declares the method.
*/
const QMetaObject *QMetaMethod::enclosingMetaObject() const { return mobj; }

namespace {

struct QueuedValues {
    std::vector<std::pair<QMetaType, void *>> values;
    QueuedValues() = default;
    QueuedValues(const QueuedValues &) = delete;
    QueuedValues &operator=(const QueuedValues &) = delete;
    ~QueuedValues() {
        for (auto &v : values)
            v.first.destroy(v.second);
    }
};

} // namespace

// Returns the number of arguments in vals if they and returnValue fit this method, which `where`
// names in a warning; otherwise returns -1.
int QMetaMethod::checkArguments(const char *where, const QGenericReturnArgument &returnValue,
                                const QGenericArgument *const *vals) const {
    const MetaMethodData &m = data();
    int n = 0;
    while (n < kMaxArgs && vals[n]->name())
        ++n;
    if (n != m.paramCount) {
        qWarning("%s: Unable to invoke %s::%s with %d arguments", where, mobj->className(), m.name, n);
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        if (!sameTypeName(vals[i]->name(), m.paramTypes[i], parameterMetaType(i))) {
            qWarning("%s: Unable to invoke %s::%s: argument %d is %s, not %s", where, mobj->className(),
                     methodSignature().constData(), i + 1, vals[i]->name(), m.paramTypes[i]);
            return -1;
        }
    }
    if (returnValue.name() && !sameTypeName(returnValue.name(), m.returnType, returnMetaType())) {
        qWarning("%s: Unable to invoke %s::%s: the return type is %s, not %s", where, mobj->className(),
                 methodSignature().constData(), m.returnType, returnValue.name());
        return -1;
    }
    if (returnValue.data() && std::strcmp(m.returnType, "void") == 0)
        return -1;
    return n;
}

/*!
    Invokes this method on \a object and returns \c true if it could be invoked. The arguments
    \a val0, \a val1, \a val2, \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9,
    made with \c Q_ARG(), must match the parameters of the method in number and type, and
    \a returnValue, made with \c Q_RETURN_ARG(), receives the return value; its type must match
    the return type of the method. See QMetaObject::invokeMethod() for \a connectionType.

    Returns \c false if the method is a constructor, if \a object is not an instance of the
    class of the method, or if the arguments do not fit.

    \sa QMetaObject::invokeMethod()
*/
bool QMetaMethod::invoke(QObject *object, Qt::ConnectionType connectionType, QGenericReturnArgument returnValue,
                         QGenericArgument val0, QGenericArgument val1, QGenericArgument val2, QGenericArgument val3,
                         QGenericArgument val4, QGenericArgument val5, QGenericArgument val6, QGenericArgument val7,
                         QGenericArgument val8, QGenericArgument val9) const {
    if (!isValid() || ctor || !object || !mobj->d.static_metacall)
        return false;
    const MetaMethodData &m = data();
    const QGenericArgument *vals[] = {&val0, &val1, &val2, &val3, &val4, &val5, &val6, &val7, &val8, &val9};
    const int n = checkArguments("QMetaMethod::invoke", returnValue, vals);
    if (n < 0)
        return false;
    if (!object->metaObject()->inherits(mobj)) {
        qWarning("QMetaMethod::invoke: %s is not an instance of %s", object->metaObject()->className(),
                 mobj->className());
        return false;
    }

    connectionType = effectiveConnectionType(object, connectionType);
    if (connectionType == Qt::BlockingQueuedConnection) {
        void *a[kMaxArgs + 1];
        a[0] = returnValue.data();
        for (int i = 0; i < n; ++i)
            a[i + 1] = vals[i]->data();
        const QMetaObject *mo = mobj;
        const int idx = index;
        // The calling thread waits, so the arguments and the return value stay valid.
        auto latch = std::make_shared<QtPrivate::BlockingLatch>();
        QCoreApplication::postEvent(
            object, new QMetaCallEvent([mo, idx, object,
                                        &a]() { mo->d.static_metacall(object, QMetaObject::InvokeMetaMethod, idx, a); },
                                       latch));
        latch->wait();
        return true;
    }
    if (connectionType == Qt::QueuedConnection) {
        if (returnValue.data()) {
            qWarning("QMetaMethod::invoke: Unable to invoke methods with return values in queued connections");
            return false;
        }
        auto copies = std::make_shared<QueuedValues>();
        for (int i = 0; i < n; ++i) {
            const QMetaType t = parameterMetaType(i);
            void *copy = t.isValid() ? t.create(vals[i]->data()) : nullptr;
            if (!copy) {
                qWarning("QMetaMethod::invoke: Unable to handle unregistered datatype '%s'; call qRegisterMetaType() "
                         "to register it",
                         m.paramTypes[i]);
                return false;
            }
            copies->values.emplace_back(t, copy);
        }
        const QMetaObject *mo = mobj;
        const int idx = index;
        QCoreApplication::postEvent(object, new QMetaCallEvent([mo, idx, object, copies]() {
                                        void *a[kMaxArgs + 1] = {nullptr};
                                        for (std::size_t i = 0; i < copies->values.size(); ++i)
                                            a[i + 1] = copies->values[i].second;
                                        mo->d.static_metacall(object, QMetaObject::InvokeMetaMethod, idx, a);
                                    }));
        return true;
    }

    void *a[kMaxArgs + 1];
    a[0] = returnValue.data();
    for (int i = 0; i < n; ++i)
        a[i + 1] = vals[i]->data();
    mobj->d.static_metacall(object, QMetaObject::InvokeMetaMethod, index, a);
    return true;
}

/*!
    \since 5.5

    Invokes this method on \a gadget, an instance of the \c Q_GADGET class that declares the
    method, and returns \c true if it could be invoked. The arguments \a val0, \a val1, \a val2,
    \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9, made with \c Q_ARG(), must
    match the parameters of the method in number and type, and \a returnValue, made with
    \c Q_RETURN_ARG(), receives the return value; its type must match the return type of the
    method. The call is always direct.

    \code
    const QMetaObject &mo = Point::staticMetaObject;
    Point point{3, 4};
    int length = 0;
    mo.method(mo.indexOfMethod("manhattanLength()")).invokeOnGadget(&point, Q_RETURN_ARG(int, length));
    \endcode

    Returns \c false if \a gadget is \c nullptr, if the method is a constructor, or if the
    arguments do not fit. The function cannot check that \a gadget is an instance of the right
    class; passing anything else is undefined behavior.

    \sa invoke(), QMetaProperty::readOnGadget()
*/
bool QMetaMethod::invokeOnGadget(void *gadget, QGenericReturnArgument returnValue, QGenericArgument val0,
                                 QGenericArgument val1, QGenericArgument val2, QGenericArgument val3,
                                 QGenericArgument val4, QGenericArgument val5, QGenericArgument val6,
                                 QGenericArgument val7, QGenericArgument val8, QGenericArgument val9) const {
    if (!isValid() || ctor || !gadget || !mobj->d.static_metacall)
        return false;
    const QGenericArgument *vals[] = {&val0, &val1, &val2, &val3, &val4, &val5, &val6, &val7, &val8, &val9};
    const int n = checkArguments("QMetaMethod::invokeOnGadget", returnValue, vals);
    if (n < 0)
        return false;
    void *a[kMaxArgs + 1];
    a[0] = returnValue.data();
    for (int i = 0; i < n; ++i)
        a[i + 1] = vals[i]->data();
    // The generated function of a gadget takes the instance through its QObject parameter.
    mobj->d.static_metacall(static_cast<QObject *>(gadget), QMetaObject::InvokeMetaMethod, index, a);
    return true;
}

/*!
    Creates a new instance with this constructor, passing the arguments \a val0, \a val1, \a val2,
    \a val3, \a val4, \a val5, \a val6, \a val7, \a val8 and \a val9, and returns it. Returns \c nullptr if this is not a constructor or the arguments do not match
    its parameters in number and type.

    \sa QMetaObject::newInstance()
*/
QObject *QMetaMethod::newInstance(QGenericArgument val0, QGenericArgument val1, QGenericArgument val2,
                                  QGenericArgument val3, QGenericArgument val4, QGenericArgument val5,
                                  QGenericArgument val6, QGenericArgument val7, QGenericArgument val8,
                                  QGenericArgument val9) const {
    if (!isValid() || !ctor || !mobj->d.static_metacall)
        return nullptr;
    const MetaMethodData &m = data();
    const QGenericArgument *vals[] = {&val0, &val1, &val2, &val3, &val4, &val5, &val6, &val7, &val8, &val9};
    int n = 0;
    while (n < kMaxArgs && vals[n]->name())
        ++n;
    if (n != m.paramCount)
        return nullptr;
    for (int i = 0; i < n; ++i)
        if (!sameTypeName(vals[i]->name(), m.paramTypes[i], parameterMetaType(i)))
            return nullptr;
    QObject *result = nullptr;
    void *a[kMaxArgs + 1];
    a[0] = static_cast<void *>(&result);
    for (int i = 0; i < n; ++i)
        a[i + 1] = vals[i]->data();
    mobj->d.static_metacall(nullptr, QMetaObject::CreateInstance, index, a);
    return result;
}

QMetaMethod QMetaMethod::fromSignalImpl(const QMetaObject *mo, const QtPrivate::SignalId &id) {
    for (const QMetaObject *m = mo; m; m = m->d.superdata) {
        const auto &data = *m->d.data;
        for (unsigned i = 0; i < data.methodCount; ++i) {
            const MetaMethodData &md = data.methods[i];
            if (md.type == Signal && md.signalId && md.signalId() == id) {
                QMetaMethod r;
                r.mobj = m;
                r.index = static_cast<int>(i);
                return r;
            }
        }
    }
    return QMetaMethod();
}

// QMetaEnum

const QtPrivate::MetaEnumData &QMetaEnum::data() const { return mobj->d.data->enums[index]; }

/*!
    Returns the name of the enumerator, without its scope.

    \sa enumName(), scope()
*/
const char *QMetaEnum::name() const { return isValid() ? data().name : nullptr; }

/*!
    Returns the name of the enumeration, without its scope. For a flags type this is the name of
    the flags type.

    \sa name()
*/
const char *QMetaEnum::enumName() const { return name(); }

/*!
    Returns the name of the class or namespace that declares the enumeration.

    \sa name()
*/
const char *QMetaEnum::scope() const { return isValid() ? mobj->className() : nullptr; }

/*!
    Returns \c true if the enumeration is a set of flags declared with \c Q_FLAG().

    \sa keysToValue()
*/
bool QMetaEnum::isFlag() const { return isValid() && data().isFlag; }

/*!
    Returns \c true if the enumeration is an \c{enum class}.
*/
bool QMetaEnum::isScoped() const { return isValid() && data().isScoped; }

/*!
    Returns the number of keys.

    \sa key(), value()
*/
int QMetaEnum::keyCount() const { return isValid() ? static_cast<int>(data().keyCount) : 0; }

/*!
    Returns the key at \a i, or \c nullptr if \a i is out of range.

    \sa value(), keyCount()
*/
const char *QMetaEnum::key(int i) const { return i >= 0 && i < keyCount() ? data().keys[i].name : nullptr; }

/*!
    Returns the value of the key at \a i, or -1 if \a i is out of range.

    \sa key()
*/
int QMetaEnum::value(int i) const { return i >= 0 && i < keyCount() ? data().keys[i].value : -1; }

/*!
    Returns the value of the key \a key, which may be qualified with the scope, such as
    \c{"Mode::Up"} or \c{"Counter::Up"}. Returns -1 if there is no such key and sets \a ok to
    \c false; otherwise sets \a ok to \c true if it is not \c nullptr.

    \sa valueToKey(), keysToValue()
*/
int QMetaEnum::keyToValue(const char *key, bool *ok) const {
    if (ok)
        *ok = false;
    if (!isValid() || !key)
        return -1;
    std::string k = key;
    if (const std::size_t colon = k.rfind("::"); colon != std::string::npos)
        k.erase(0, colon + 2);
    for (int i = 0; i < keyCount(); ++i) {
        if (k == data().keys[i].name) {
            if (ok)
                *ok = true;
            return data().keys[i].value;
        }
    }
    return -1;
}

/*!
    Returns the key of the first enumerator with the value \a value, or \c nullptr if there is
    none.

    \sa keyToValue(), valueToKeys()
*/
const char *QMetaEnum::valueToKey(int value) const {
    for (int i = 0; i < keyCount(); ++i)
        if (data().keys[i].value == value)
            return data().keys[i].name;
    return nullptr;
}

/*!
    Returns the combined value of the keys in \a keys, which are separated by \c{|}, such as
    \c{"Read|Write"}. Returns -1 and sets \a ok to \c false if a key is unknown; otherwise sets
    \a ok to \c true if it is not \c nullptr.

    \sa valueToKeys(), keyToValue()
*/
int QMetaEnum::keysToValue(const char *keys, bool *ok) const {
    if (ok)
        *ok = false;
    if (!isValid() || !keys)
        return -1;
    int result = 0;
    const std::string all = keys;
    std::size_t start = 0;
    while (start <= all.size()) {
        std::size_t bar = all.find('|', start);
        if (bar == std::string::npos)
            bar = all.size();
        std::string key = all.substr(start, bar - start);
        const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
        key.erase(key.begin(), std::find_if_not(key.begin(), key.end(), isSpace));
        key.erase(std::find_if_not(key.rbegin(), key.rend(), isSpace).base(), key.end());
        if (key.empty() && all.find_first_not_of(" \t") == std::string::npos)
            return -1;
        bool found = false;
        const int v = keyToValue(key.c_str(), &found);
        if (!found)
            return -1;
        result |= v;
        start = bar + 1;
    }
    if (ok)
        *ok = true;
    return result;
}

/*!
    Returns the keys whose values are set in \a value, separated by \c{|}. A value of 0 gives
    the key whose value is 0, if there is one. Bits that no key accounts for are ignored.

    \sa keysToValue(), valueToKey()
*/
QByteArray QMetaEnum::valueToKeys(int value) const {
    QByteArray keys;
    if (!isValid())
        return keys;
    if (value == 0) {
        const char *k = valueToKey(0);
        return k ? QByteArray(k) : QByteArray();
    }
    for (int i = 0; i < keyCount(); ++i) {
        const int v = data().keys[i].value;
        if (v != 0 && (value & v) == v) {
            if (!keys.isEmpty())
                keys.append('|');
            keys.append(data().keys[i].name);
        }
    }
    return keys;
}

/*!
    Returns the meta-object of the class or namespace that declares the enumeration.
*/
const QMetaObject *QMetaEnum::enclosingMetaObject() const { return mobj; }

// QMetaClassInfo

const QtPrivate::MetaClassInfoData &QMetaClassInfo::data() const { return mobj->d.data->classInfos[index]; }

/*!
    Returns the name of the entry.

    \sa value()
*/
const char *QMetaClassInfo::name() const { return isValid() ? data().name : nullptr; }

/*!
    Returns the value of the entry.

    \sa name()
*/
const char *QMetaClassInfo::value() const { return isValid() ? data().value : nullptr; }

/*!
    Returns the meta-object of the class that declares the entry.
*/
const QMetaObject *QMetaClassInfo::enclosingMetaObject() const { return mobj; }

// QMetaProperty

const QtPrivate::MetaPropertyData &QMetaProperty::data() const { return mobj->d.data->properties[index]; }

/*!
    Returns the name of the property.

    \sa typeName()
*/
const char *QMetaProperty::name() const { return isValid() ? data().name : nullptr; }

/*!
    Returns the name of the type of the property as it is declared, such as \c{"int"} or
    \c{"Counter::Mode"}.

    \sa metaType()
*/
const char *QMetaProperty::typeName() const { return isValid() ? data().typeName : nullptr; }

/*!
    Returns the QMetaType of the property, or an invalid QMetaType if the type is not known.

    \sa typeId()
*/
QMetaType QMetaProperty::metaType() const {
    if (!isValid())
        return QMetaType();
    QMetaType t;
    if (mobj->d.static_metacall) {
        void *a[] = {&t};
        mobj->d.static_metacall(nullptr, QMetaObject::RegisterPropertyMetaType, index, a);
    }
    return t.isValid() ? t : QMetaType::fromName(QByteArrayView(data().typeName));
}

/*!
    Returns the \c QMetaType id of the property, or \c QMetaType::UnknownType if the type is not
    known.

    \sa metaType()
*/
int QMetaProperty::typeId() const { return metaType().id(); }

/*!
    \fn int QMetaProperty::userType() const
    Returns the \c QMetaType id of the property; the same as typeId().
*/

/*!
    Returns the index of the property, counting the properties of the base classes first.

    \sa relativePropertyIndex()
*/
int QMetaProperty::propertyIndex() const { return isValid() ? mobj->propertyOffset() + index : -1; }

/*!
    Returns the index of the property among those of its own class.

    \sa propertyIndex()
*/
int QMetaProperty::relativePropertyIndex() const { return isValid() ? index : -1; }

/*!
    Returns \c true if the property can be read.

    \sa read(), isWritable()
*/
bool QMetaProperty::isReadable() const { return isValid() && (data().flags & QtPrivate::PropReadable); }

/*!
    Returns \c true if the property can be written.

    \sa write(), isReadable()
*/
bool QMetaProperty::isWritable() const { return isValid() && (data().flags & QtPrivate::PropWritable); }

/*!
    Returns \c true if the property has a \c RESET function.

    \sa reset()
*/
bool QMetaProperty::isResettable() const { return isValid() && (data().flags & QtPrivate::PropResettable); }

bool QMetaProperty::queryFlag(unsigned shift, const QObject *obj, QMetaObject::Call call, bool defaultValue) const {
    if (!isValid())
        return defaultValue;
    const unsigned v = (data().flags >> shift) & 0x3u;
    if (v < 2)
        return v == 1;
    if (!obj || !mobj->d.static_metacall)
        return defaultValue;
    bool result = defaultValue;
    void *a[] = {&result};
    mobj->d.static_metacall(const_cast<QObject *>(obj), call, index, a);
    return result;
}

/*!
    Returns \c true if the property is designable for \a obj, which is the value of its
    \c DESIGNABLE attribute. The default is \c true. If the attribute is a function and
    \a obj is \c nullptr, returns \c true.

    \sa isScriptable(), isStored()
*/
bool QMetaProperty::isDesignable(const QObject *obj) const {
    return queryFlag(QtPrivate::PropDesignableShift, obj, QMetaObject::QueryPropertyDesignable, true);
}

/*!
    Returns \c true if the property is scriptable for \a obj, which is the value of its
    \c SCRIPTABLE attribute. The default is \c true.

    \sa isDesignable(), isStored()
*/
bool QMetaProperty::isScriptable(const QObject *obj) const {
    return queryFlag(QtPrivate::PropScriptableShift, obj, QMetaObject::QueryPropertyScriptable, true);
}

/*!
    Returns \c true if the property is stored for \a obj, which is the value of its \c STORED
    attribute. The default is \c true.

    \sa isDesignable(), isUser()
*/
bool QMetaProperty::isStored(const QObject *obj) const {
    return queryFlag(QtPrivate::PropStoredShift, obj, QMetaObject::QueryPropertyStored, true);
}

/*!
    Returns \c true if the property is the user property of \a obj, which is the value of its
    \c USER attribute. The default is \c false.

    \sa QMetaObject::userProperty()
*/
bool QMetaProperty::isUser(const QObject *obj) const {
    return queryFlag(QtPrivate::PropUserShift, obj, QMetaObject::QueryPropertyUser, false);
}

/*!
    Returns \c true if the property is declared \c CONSTANT, so its value does not change for
    an object.

    \sa isFinal()
*/
bool QMetaProperty::isConstant() const { return isValid() && (data().flags & QtPrivate::PropConstant); }

/*!
    Returns \c true if the property is declared \c FINAL, so no derived class overrides it.

    \sa isConstant()
*/
bool QMetaProperty::isFinal() const { return isValid() && (data().flags & QtPrivate::PropFinal); }

/*!
    Returns \c true if the property is declared \c REQUIRED.

    \sa isConstant()
*/
bool QMetaProperty::isRequired() const { return isValid() && (data().flags & QtPrivate::PropRequired); }

/*!
    Returns \c true if the type of the property is an enumeration declared with \c Q_FLAG().

    \sa isEnumType()
*/
bool QMetaProperty::isFlagType() const { return enumerator().isFlag(); }

/*!
    Returns \c true if the type of the property is an enumeration that has meta-data.

    \sa enumerator()
*/
bool QMetaProperty::isEnumType() const { return enumerator().isValid(); }

/*!
    Returns \c false: bindable properties are not supported.
*/
bool QMetaProperty::isBindable() const { return false; }

/*!
    Returns the enumerator that describes the type of the property, or an invalid QMetaEnum if
    its type is not an enumeration with meta-data.

    \sa isEnumType()
*/
QMetaEnum QMetaProperty::enumerator() const {
    if (!isValid())
        return QMetaEnum();
    std::string name = data().typeName;
    if (const std::size_t colon = name.rfind("::"); colon != std::string::npos)
        name.erase(0, colon + 2);
    int idx = mobj->indexOfEnumerator(name.c_str());
    if (idx >= 0)
        return mobj->enumerator(idx);
    const QMetaObject *other = metaType().metaObject();
    if (other) {
        idx = other->indexOfEnumerator(name.c_str());
        if (idx >= 0)
            return other->enumerator(idx);
    }
    return QMetaEnum();
}

/*!
    Returns \c true if the property has a \c NOTIFY signal.

    \sa notifySignal()
*/
bool QMetaProperty::hasNotifySignal() const { return isValid() && data().notifySignal >= 0; }

/*!
    Returns the \c NOTIFY signal of the property, or an invalid QMetaMethod if it has none.

    \sa hasNotifySignal()
*/
QMetaMethod QMetaProperty::notifySignal() const {
    QMetaMethod m;
    if (hasNotifySignal()) {
        m.mobj = mobj;
        m.index = data().notifySignal;
    }
    return m;
}

/*!
    Returns the index of the \c NOTIFY signal in its class, counting the methods of the base
    classes first, or -1 if the property has none.

    \sa notifySignal()
*/
int QMetaProperty::notifySignalIndex() const {
    return hasNotifySignal() ? mobj->methodOffset() + data().notifySignal : -1;
}

/*!
    Returns the revision of the property, which is 0 unless it is declared with \c REVISION.
*/
int QMetaProperty::revision() const { return isValid() ? data().revision : 0; }

/*!
    Returns the meta-object of the class that declares the property.
*/
const QMetaObject *QMetaProperty::enclosingMetaObject() const { return mobj; }

QVariant QMetaProperty::readImpl(QObject *o) const {
    if (!isReadable() || !mobj->d.static_metacall)
        return QVariant();
    const QMetaType type = metaType();
    if (!type.isValid())
        return QVariant();
    if (type.id() == QMetaType::QVariant) {
        QVariant value;
        void *a[] = {&value};
        mobj->d.static_metacall(o, QMetaObject::ReadProperty, index, a);
        return value;
    }
    QVariant value(type);
    if (!value.isValid())
        return QVariant();
    void *a[] = {value.data()};
    mobj->d.static_metacall(o, QMetaObject::ReadProperty, index, a);
    return value;
}

bool QMetaProperty::writeImpl(QObject *o, const QVariant &value) const {
    if (!isWritable() || !mobj->d.static_metacall)
        return false;
    const QMetaType type = metaType();
    if (!type.isValid())
        return false;
    if (type.id() == QMetaType::QVariant) {
        void *a[] = {const_cast<QVariant *>(&value)};
        mobj->d.static_metacall(o, QMetaObject::WriteProperty, index, a);
        return true;
    }
    QVariant v = value;
    if (!v.isValid()) {
        return isResettable() && reset(o);
    }
    if (v.metaType() != type && !v.convert(type))
        return false;
    void *a[] = {v.data()};
    mobj->d.static_metacall(o, QMetaObject::WriteProperty, index, a);
    return true;
}

/*!
    Reads the property of \a obj and returns it as a QVariant. Returns an invalid variant if the
    property is not readable or its type is not registered.

    \sa write(), isReadable()
*/
QVariant QMetaProperty::read(const QObject *obj) const {
    return obj ? readImpl(const_cast<QObject *>(obj)) : QVariant();
}

/*!
    Writes \a value to the property of \a obj and returns \c true on success. \a value is
    converted to the type of the property first; for an enumeration it can also be the name of
    a key. Returns \c false if the property is not writable or the value cannot be converted.
    Writing an invalid variant resets a resettable property.

    \sa read(), reset(), isWritable()
*/
bool QMetaProperty::write(QObject *obj, const QVariant &value) const { return obj && writeImpl(obj, value); }

/*!
    Resets the property of \a obj with its \c RESET function and returns \c true, or returns
    \c false if the property cannot be reset.

    \sa isResettable()
*/
bool QMetaProperty::reset(QObject *obj) const {
    if (!obj || !isResettable() || !mobj->d.static_metacall)
        return false;
    mobj->d.static_metacall(obj, QMetaObject::ResetProperty, index, nullptr);
    return true;
}

/*!
    Reads the property of the \c Q_GADGET value at \a gadget.

    \sa read()
*/
QVariant QMetaProperty::readOnGadget(const void *gadget) const {
    return gadget ? readImpl(static_cast<QObject *>(const_cast<void *>(gadget))) : QVariant();
}

/*!
    Writes \a value to the property of the \c Q_GADGET value at \a gadget and returns \c true on
    success.

    \sa write()
*/
bool QMetaProperty::writeOnGadget(void *gadget, const QVariant &value) const {
    return gadget && writeImpl(static_cast<QObject *>(gadget), value);
}

/*!
    Resets the property of the \c Q_GADGET value at \a gadget and returns \c true on success.

    \sa reset()
*/
bool QMetaProperty::resetOnGadget(void *gadget) const {
    if (!gadget || !isResettable() || !mobj->d.static_metacall)
        return false;
    mobj->d.static_metacall(static_cast<QObject *>(gadget), QMetaObject::ResetProperty, index, nullptr);
    return true;
}

/*!
    Returns \c true if the property has a setter that follows the \c setName() convention.
    Always \c true for a writable property of Nt.
*/
bool QMetaProperty::hasStdCppSet() const { return isWritable(); }

// QObject

/*!
    Returns the meta-object of the class of this object. The result is that of the dynamic type,
    so it describes the derived class even when it is called through a base class pointer. A
    class that \c ntmoc did not process is described by its nearest base class that it did.

    \sa QMetaObject, inherits()
*/
const QMetaObject *QObject::metaObject() const { return QtPrivate::findMetaObjectOf(this); }

/*!
    Returns \c true if this object is an instance of the class \a className or of a class that
    inherits from it; otherwise returns \c false. The class name is that of the meta-object, so it
    is qualified with its namespaces.

    \sa metaObject(), QMetaObject::inherits()
*/
bool QObject::inherits(const char *className) const {
    if (!className)
        return false;
    for (const QMetaObject *m = metaObject(); m; m = m->superClass())
        if (std::strcmp(m->className(), className) == 0)
            return true;
    return false;
}

/*!
    Returns the value of the property \a name, or an invalid QVariant if there is no such
    property. A property that \c Q_PROPERTY() declares is read through its \c READ function;
    otherwise the dynamic property of that name is returned.

    \sa setProperty(), dynamicPropertyNames(), QMetaProperty::read()
*/
QVariant QObject::property(const char *name) const {
    if (!name)
        return QVariant();
    const QMetaObject *mo = metaObject();
    const int idx = mo->indexOfProperty(name);
    if (idx >= 0)
        return mo->property(idx).read(this);
    for (const auto &p : d_ptr->dynamicProperties)
        if (p.first == name)
            return p.second;
    return QVariant();
}

/*!
    Sets the property \a name to \a value. Returns \c true if the property is declared with
    \c Q_PROPERTY() and the value was written. For a name that is not declared, a dynamic
    property is created or changed and a QDynamicPropertyChangeEvent is sent to the object, and
    the function returns \c false; setting an invalid \a value removes a dynamic property.

    \sa property(), dynamicPropertyNames(), QMetaProperty::write()
*/
bool QObject::setProperty(const char *name, const QVariant &value) {
    if (!name)
        return false;
    const QMetaObject *mo = metaObject();
    const int idx = mo->indexOfProperty(name);
    if (idx >= 0) {
        const QMetaProperty p = mo->property(idx);
        if (!p.isWritable() && !(p.isResettable() && !value.isValid())) {
            qWarning("QObject::setProperty: Property \"%s\" invalid, read-only or does not exist", name);
            return false;
        }
        return p.write(this, value);
    }
    auto &props = d_ptr->dynamicProperties;
    const auto it = std::find_if(props.begin(), props.end(), [name](const auto &p) { return p.first == name; });
    if (!value.isValid()) {
        if (it == props.end())
            return false;
        props.erase(it);
    } else if (it != props.end()) {
        it->second = value;
    } else {
        props.emplace_back(QByteArray(name), value);
    }
    QDynamicPropertyChangeEvent event{QByteArray(name)};
    QCoreApplication::sendEvent(this, &event);
    return false;
}

/*!
    Returns the names of the dynamic properties of the object, in the order they were created.

    \sa setProperty()
*/
QList<QByteArray> QObject::dynamicPropertyNames() const {
    QList<QByteArray> names;
    for (const auto &p : d_ptr->dynamicProperties)
        names.append(p.first);
    return names;
}

namespace {

// Splits a signature that starts with a code ('0', '1' or '2') and checks the code.
bool checkCode(const char *text, const char *what, std::initializer_list<char> codes, char &code) {
    if (!text || !*text) {
        qWarning("QObject::connect: Cannot connect with an empty %s", what);
        return false;
    }
    code = text[0];
    for (char c : codes)
        if (c == code)
            return true;
    qWarning("QObject::connect: Use the %s macro to name the %s %s", code == '1' ? "SLOT" : "SIGNAL", what, text);
    return false;
}

} // namespace

/*!
    Connects \a signal of \a sender to \a member of \a receiver and returns a handle to the
    connection, or an invalid connection if it cannot be made. Name the signal with the
    \c SIGNAL() macro, and the member with \c SLOT() or \c SIGNAL(); the arguments of the member
    must be those of the signal, or a prefix of them:

    \code
    QObject::connect(&counter, SIGNAL(valueChanged(int)), &display, SLOT(show(int)));
    \endcode

    A warning is printed and no connection is made if either object is \c nullptr, the signal or
    the member does not exist, or their arguments are not compatible. \a type selects direct or
    queued delivery.

    \sa disconnect(), {Signals and Slots}
*/
QMetaObject::Connection QObject::connect(const QObject *sender, const char *signal, const QObject *receiver,
                                         const char *member, Qt::ConnectionType type) {
    if (!sender || !signal || !receiver || !member) {
        qWarning("QObject::connect: Cannot connect %s::%s to %s::%s",
                 sender ? sender->metaObject()->className() : "(nullptr)", signal ? signal : "(nullptr)",
                 receiver ? receiver->metaObject()->className() : "(nullptr)", member ? member : "(nullptr)");
        return {};
    }
    char signalCode = 0;
    char memberCode = 0;
    if (!checkCode(signal, "signal", {'2'}, signalCode) || !checkCode(member, "member", {'0', '1', '2'}, memberCode))
        return {};
    const QMetaObject *smo = sender->metaObject();
    const QMetaObject *rmo = receiver->metaObject();
    const std::string sigName = NtMeta::normalizeSignature(signal + 1);
    const std::string memName = NtMeta::normalizeSignature(member + 1);
    const int signalIndex = smo->indexOfSignal(sigName.c_str());
    if (signalIndex < 0) {
        qWarning("QObject::connect: No such signal %s::%s", smo->className(), sigName.c_str());
        return {};
    }
    int methodIndex = -1;
    if (memberCode == '1')
        methodIndex = rmo->indexOfSlot(memName.c_str());
    else if (memberCode == '2')
        methodIndex = rmo->indexOfSignal(memName.c_str());
    else
        methodIndex = rmo->indexOfMethod(memName.c_str());
    if (methodIndex < 0) {
        qWarning("QObject::connect: No such %s %s::%s", memberCode == '2' ? "signal" : "slot", rmo->className(),
                 memName.c_str());
        return {};
    }
    const QMetaMethod s = smo->method(signalIndex);
    const QMetaMethod m = rmo->method(methodIndex);
    if (!QMetaObject::checkConnectArgs(s, m)) {
        qWarning("QObject::connect: Incompatible sender/receiver arguments %s::%s --> %s::%s", smo->className(),
                 sigName.c_str(), rmo->className(), memName.c_str());
        return {};
    }
    return connect(sender, s, receiver, m, type);
}

/*!
    \overload
    Connects the signal \a signal of \a sender to the method \a method of \a receiver. Get the
    two QMetaMethod objects from the meta-objects of the classes, or with
    QMetaMethod::fromSignal().
*/
QMetaObject::Connection QObject::connect(const QObject *sender, const QMetaMethod &signal, const QObject *receiver,
                                         const QMetaMethod &method, Qt::ConnectionType type) {
    if (!sender || !receiver || !signal.isValid() || !method.isValid() || signal.methodType() != QMetaMethod::Signal ||
        method.methodType() == QMetaMethod::Constructor || !signal.data().signalId) {
        qWarning("QObject::connect: Cannot connect %s::%s to %s::%s",
                 sender ? sender->metaObject()->className() : "(nullptr)",
                 signal.isValid() ? signal.methodSignature().constData() : "(invalid)",
                 receiver ? receiver->metaObject()->className() : "(nullptr)",
                 method.isValid() ? method.methodSignature().constData() : "(invalid)");
        return {};
    }
    if (!sender->metaObject()->inherits(signal.enclosingMetaObject()) ||
        !receiver->metaObject()->inherits(method.enclosingMetaObject())) {
        qWarning("QObject::connect: The methods do not belong to the objects");
        return {};
    }
    if (!QMetaObject::checkConnectArgs(signal, method)) {
        qWarning("QObject::connect: Incompatible sender/receiver arguments %s --> %s",
                 signal.methodSignature().constData(), method.methodSignature().constData());
        return {};
    }
    const QMetaObject *mo = method.enclosingMetaObject();
    const int idx = method.relativeMethodIndex();
    const int argc = method.parameterCount();
    auto *target = const_cast<QObject *>(receiver);
    auto invoke = [mo, idx, argc, target](void *const *argv) {
        void *a[kMaxArgs + 1];
        a[0] = nullptr;
        for (int i = 0; i < argc; ++i)
            a[i + 1] = argv[i];
        mo->d.static_metacall(target, QMetaObject::InvokeMetaMethod, idx, a);
    };
    const std::string signature = method.methodSignature().constData();
    return connectImpl(sender, signal.data().signalId(), target, std::function<void(void *const *)>(std::move(invoke)),
                       type, nullptr, &signature);
}

QMetaObject::Connection QObject::connectFunction(const QObject *sender, const QMetaMethod &signal, QObject *context,
                                                 std::function<void(void *const *)> function) {
    if (!sender || !signal.isValid() || signal.methodType() != QMetaMethod::Signal)
        return {};
    return connectImpl(sender, signal.data().signalId(), context, std::move(function), Qt::DirectConnection);
}

QMetaObject::Connection QtPrivate::Access::connectFunction(const QObject *sender, const QMetaMethod &signal,
                                                           QObject *context,
                                                           std::function<void(void *const *)> function) {
    return QObject::connectFunction(sender, signal, context, std::move(function));
}

/*!
    Removes the connections from \a signal of \a sender to \a member of \a receiver and returns
    \c true if any was removed. \a signal and \a member are named with \c SIGNAL() and \c SLOT().
    A \c nullptr \a signal matches every signal, a \c nullptr \a receiver every receiver, and a
    \c nullptr \a member every member of the receiver.

    \sa connect()
*/
bool QObject::disconnect(const QObject *sender, const char *signal, const QObject *receiver, const char *member) {
    if (!sender)
        return false;
    if (member && !receiver) {
        qWarning("QObject::disconnect: Cannot disconnect a member without a receiver");
        return false;
    }
    const QtPrivate::SignalId *id = nullptr;
    QtPrivate::SignalId signalId;
    std::string memberSig;
    if (signal) {
        if (signal[0] != '2') {
            qWarning("QObject::disconnect: Use the SIGNAL macro to name the signal %s", signal);
            return false;
        }
        const QMetaObject *smo = sender->metaObject();
        const int idx = smo->indexOfSignal(NtMeta::normalizeSignature(signal + 1).c_str());
        if (idx < 0) {
            qWarning("QObject::disconnect: No such signal %s::%s", smo->className(), signal + 1);
            return false;
        }
        const QMetaMethod method = smo->method(idx);
        if (!method.isValid() || !method.data().signalId)
            return false;
        signalId = method.data().signalId();
        id = &signalId;
    }
    if (member)
        memberSig = NtMeta::normalizeSignature(member + 1);
    return QtPrivate::disconnectConnections(sender, id, receiver, member ? &memberSig : nullptr);
}

/*!
    \overload
    Removes the connections from \a signal of \a sender to \a method of \a receiver. An invalid
    \a signal matches every signal, and an invalid \a method every method of the receiver.
*/
bool QObject::disconnect(const QObject *sender, const QMetaMethod &signal, const QObject *receiver,
                         const QMetaMethod &method) {
    if (!sender)
        return false;
    const std::string memberSig = method.isValid() ? std::string(method.methodSignature().constData()) : std::string();
    if (method.isValid() && !receiver) {
        qWarning("QObject::disconnect: Cannot disconnect a method without a receiver");
        return false;
    }
    QtPrivate::SignalId id;
    if (signal.isValid()) {
        if (signal.methodType() != QMetaMethod::Signal || !signal.data().signalId)
            return false;
        id = signal.data().signalId();
    }
    return QtPrivate::disconnectConnections(sender, signal.isValid() ? &id : nullptr, receiver,
                                            method.isValid() ? &memberSig : nullptr);
}

/*!
    \overload
    Removes the connections from \a signal of this object to \a member of \a receiver, named
    with \c SIGNAL() and \c SLOT(). With no arguments, removes every connection from this
    object.
*/
bool QObject::disconnect(const char *signal, const QObject *receiver, const char *member) const {
    return disconnect(this, signal, receiver, member);
}

/*!
    \overload
    Removes the connections from this object to \a receiver, or only those to \a member, which
    is named with \c SLOT().
*/
bool QObject::disconnect(const QObject *receiver, const char *member) const {
    if (!member)
        return disconnectImpl(this, nullptr, receiver);
    return disconnect(this, nullptr, receiver, member);
}
