// MIT License - see LICENSE
#include "qdbusmarshaller_p.h"

#include <QtDBus/qdbusmetatype.h>

#include <map>
#include <mutex>
#include <string>

/*!
    \class QDBusMetaType
    \brief The QDBusMetaType class allows you to register types for use with D-Bus.

    The basic D-Bus types, QStringList, QByteArray, QVariantList, QVariantMap and lists of the
    numbers and of QDBusObjectPath are known from the start. Register other types with
    qDBusRegisterMetaType():

    \code
    qDBusRegisterMetaType<Fix>();
    qDBusRegisterMetaType<QList<Fix>>();
    \endcode

    \differences
    \c QList<bool> cannot be registered, because Nt rejects it at compile time; an array of
    booleans arrives as a QDBusArgument.
    \enddifferences

    \sa QDBusArgument
*/

namespace {

struct Entry {
    QDBusMetaType::MarshallFunction marshall = nullptr;
    QDBusMetaType::DemarshallFunction demarshall = nullptr;
    std::string signature;
    bool computed = false;
};

template <typename T> void builtin(std::map<int, Entry> &map) {
    Entry e;
    e.marshall = [](QDBusArgument &arg, const void *t) { arg << *static_cast<const T *>(t); };
    e.demarshall = [](const QDBusArgument &arg, void *t) { arg >> *static_cast<T *>(t); };
    map[QMetaType::fromType<T>().id()] = e;
}

struct Registry {
    std::recursive_mutex mutex;
    std::map<int, Entry> entries;

    Registry() {
        builtin<QVariantList>(entries);
        builtin<QVariantMap>(entries);
        builtin<QVariantHash>(entries);
        builtin<QList<short>>(entries);
        builtin<QList<ushort>>(entries);
        builtin<QList<int>>(entries);
        builtin<QList<uint>>(entries);
        builtin<QList<qlonglong>>(entries);
        builtin<QList<qulonglong>>(entries);
        builtin<QList<double>>(entries);
        builtin<QList<QDBusVariant>>(entries);
        builtin<QList<QDBusObjectPath>>(entries);
        builtin<QList<QDBusSignature>>(entries);
        builtin<QList<QDBusUnixFileDescriptor>>(entries);
        builtin<QMap<QString, QString>>(entries);
    }
};

Registry &registry() {
    static Registry r;
    return r;
}

const char *basicSignature(QMetaType type) {
    switch (type.id()) {
    case QMetaType::UChar:
        return "y";
    case QMetaType::Bool:
        return "b";
    case QMetaType::Short:
        return "n";
    case QMetaType::UShort:
        return "q";
    case QMetaType::Int:
        return "i";
    case QMetaType::UInt:
        return "u";
    case QMetaType::LongLong:
    case QMetaType::Long:
        return "x";
    case QMetaType::ULongLong:
    case QMetaType::ULong:
        return "t";
    case QMetaType::Double:
        return "d";
    case QMetaType::QString:
        return "s";
    case QMetaType::QStringList:
        return "as";
    case QMetaType::QByteArray:
        return "ay";
    case QMetaType::QVariant:
        return "v";
    default:
        break;
    }
    if (type == QMetaType::fromType<QDBusVariant>())
        return "v";
    if (type == QMetaType::fromType<QDBusObjectPath>())
        return "o";
    if (type == QMetaType::fromType<QDBusSignature>())
        return "g";
    if (type == QMetaType::fromType<QDBusUnixFileDescriptor>())
        return "h";
    return nullptr;
}

} // namespace

/*!
    Registers \a marshall and \a demarshall as the functions that write and read values of the
    type \a metaType. qDBusRegisterMetaType() calls this function.
*/
void QDBusMetaType::registerMarshallOperators(QMetaType metaType, MarshallFunction marshall,
                                              DemarshallFunction demarshall) {
    if (!metaType.isValid() || !marshall || !demarshall)
        return;
    Registry &r = registry();
    std::lock_guard<std::recursive_mutex> lock(r.mutex);
    Entry &e = r.entries[metaType.id()];
    e.marshall = marshall;
    e.demarshall = demarshall;
    e.computed = false;
    e.signature.clear();
}

/*!
    Writes the value of the type \a metaType at \a data to \a argument. Returns \c false if the
    type is not registered.
*/
bool QDBusMetaType::marshall(QDBusArgument &argument, QMetaType metaType, const void *data) {
    MarshallFunction f = nullptr;
    {
        Registry &r = registry();
        std::lock_guard<std::recursive_mutex> lock(r.mutex);
        const auto it = r.entries.find(metaType.id());
        if (it != r.entries.end())
            f = it->second.marshall;
    }
    if (!f)
        return false;
    f(argument, data);
    return true;
}

/*!
    Reads a value of the type \a metaType from \a argument into \a data, which must hold a
    constructed value of that type. Returns \c false if the type is not registered.
*/
bool QDBusMetaType::demarshall(const QDBusArgument &argument, QMetaType metaType, void *data) {
    DemarshallFunction f = nullptr;
    {
        Registry &r = registry();
        std::lock_guard<std::recursive_mutex> lock(r.mutex);
        const auto it = r.entries.find(metaType.id());
        if (it != r.entries.end())
            f = it->second.demarshall;
    }
    if (!f)
        return false;
    f(argument, data);
    return true;
}

/*!
    Returns the meta-type that values with the D-Bus \a signature are read into: the basic types
    and the types registered with QDBusMetaType, or QDBusArgument for any other valid single
    type. Returns an invalid QMetaType if \a signature is not a single complete type.
*/
QMetaType QDBusMetaType::signatureToMetaType(const char *signature) {
    const QByteArray sig(signature ? signature : "");
    if (!QDBusUtil::isValidSingleSignature(sig))
        return QMetaType();
    struct Basic {
        const char *sig;
        QMetaType type;
    };
    const Basic basics[] = {{"y", QMetaType::fromType<uchar>()},
                            {"b", QMetaType::fromType<bool>()},
                            {"n", QMetaType::fromType<short>()},
                            {"q", QMetaType::fromType<ushort>()},
                            {"i", QMetaType::fromType<int>()},
                            {"u", QMetaType::fromType<uint>()},
                            {"x", QMetaType::fromType<qlonglong>()},
                            {"t", QMetaType::fromType<qulonglong>()},
                            {"d", QMetaType::fromType<double>()},
                            {"s", QMetaType::fromType<QString>()},
                            {"o", QMetaType::fromType<QDBusObjectPath>()},
                            {"g", QMetaType::fromType<QDBusSignature>()},
                            {"h", QMetaType::fromType<QDBusUnixFileDescriptor>()},
                            {"v", QMetaType::fromType<QDBusVariant>()},
                            {"as", QMetaType::fromType<QStringList>()},
                            {"ay", QMetaType::fromType<QByteArray>()},
                            {"av", QMetaType::fromType<QVariantList>()},
                            {"a{sv}", QMetaType::fromType<QVariantMap>()}};
    for (const Basic &b : basics)
        if (sig == b.sig)
            return b.type;
    std::vector<int> ids;
    {
        Registry &r = registry();
        std::lock_guard<std::recursive_mutex> lock(r.mutex);
        for (const auto &e : r.entries)
            ids.push_back(e.first);
    }
    for (int id : ids)
        if (sig == typeToSignature(QMetaType(id)))
            return QMetaType(id);
    return QMetaType::fromType<QDBusArgument>();
}

/*!
    Returns the D-Bus signature of values of the type \a metaType, or \c nullptr if the type is
    not known to the D-Bus type system.
*/
const char *QDBusMetaType::typeToSignature(QMetaType metaType) {
    if (const char *basic = basicSignature(metaType))
        return basic;
    Registry &r = registry();
    std::lock_guard<std::recursive_mutex> lock(r.mutex);
    const auto it = r.entries.find(metaType.id());
    if (it == r.entries.end())
        return nullptr;
    if (!it->second.computed) {
        // The signature is what marshalling a default value writes.
        QDBusWriter writer;
        void *value = metaType.create();
        if (!value)
            return nullptr;
        QDBusArgument arg = QDBusArgumentPrivate::forWriter(&writer);
        it->second.marshall(arg, value);
        metaType.destroy(value);
        if (!QDBusUtil::isValidSingleSignature(writer.signature))
            return nullptr;
        it->second.signature = writer.signature.toStdString();
        it->second.computed = true;
    }
    return it->second.signature.c_str();
}
