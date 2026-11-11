// MIT License - see LICENSE
#include <QtCore/qcborarray.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qbitarray.h>
#include <QtCore/qline.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qcompare.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qlocale.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qvariant.h>
#include <QtCore/qversionnumber.h>

#include "ntnormalize_p.h"
#include "qmetatype_p.h"

#include <array>
#include <cstdlib>
#include <mutex>

namespace {

constexpr int kBuiltinCount = 64;

struct Registry {
    std::mutex mutex;
    std::vector<const QMetaTypeInterface *> custom; // index is id - QMetaType::User
    std::deque<std::string> names;                  // primary names, parallel to custom
    std::unordered_map<std::string, int> byName;    // every registered name and alias
    std::map<std::pair<int, int>, QMetaType::ConverterFunction> converters;
};

// Never destroyed: variants in static objects may use the registry while the program exits.
Registry &registry() {
    static Registry *r = new Registry; // NOLINT(cppcoreguidelines-owning-memory)
    return *r;
}

const QMetaTypeInterface *voidInterface() {
    static const QMetaTypeInterface iface = {{QMetaType::Void}, 0,       0,       0,       "void",  nullptr, nullptr,
                                             nullptr,           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                             nullptr,           nullptr, nullptr, nullptr, nullptr, nullptr};
    return &iface;
}

struct BuiltinName {
    int id;
    const char *name;
};

const BuiltinName kBuiltinNames[] = {
    {QMetaType::Bool, "bool"},
    {QMetaType::Int, "int"},
    {QMetaType::UInt, "uint"},
    {QMetaType::LongLong, "qlonglong"},
    {QMetaType::ULongLong, "qulonglong"},
    {QMetaType::Double, "double"},
    {QMetaType::QChar, "QChar"},
    {QMetaType::QVariantMap, "QVariantMap"},
    {QMetaType::QVariantList, "QVariantList"},
    {QMetaType::QString, "QString"},
    {QMetaType::QStringList, "QStringList"},
    {QMetaType::QByteArray, "QByteArray"},
    {QMetaType::QBitArray, "QBitArray"},
    {QMetaType::QDate, "QDate"},
    {QMetaType::QTime, "QTime"},
    {QMetaType::QDateTime, "QDateTime"},
    {QMetaType::QUrl, "QUrl"},
    {QMetaType::QLocale, "QLocale"},
    {QMetaType::QRect, "QRect"},
    {QMetaType::QRectF, "QRectF"},
    {QMetaType::QSize, "QSize"},
    {QMetaType::QSizeF, "QSizeF"},
    {QMetaType::QLine, "QLine"},
    {QMetaType::QLineF, "QLineF"},
    {QMetaType::QPoint, "QPoint"},
    {QMetaType::QPointF, "QPointF"},
    {QMetaType::QRegularExpression, "QRegularExpression"},
    {QMetaType::QVariantHash, "QVariantHash"},
    {QMetaType::QEasingCurve, "QEasingCurve"},
    {QMetaType::QUuid, "QUuid"},
    {QMetaType::VoidStar, "void*"},
    {QMetaType::Long, "long"},
    {QMetaType::Short, "short"},
    {QMetaType::Char, "char"},
    {QMetaType::ULong, "ulong"},
    {QMetaType::UShort, "ushort"},
    {QMetaType::UChar, "uchar"},
    {QMetaType::Float, "float"},
    {QMetaType::QObjectStar, "QObject*"},
    {QMetaType::SChar, "signed char"},
    {QMetaType::QVariant, "QVariant"},
    {QMetaType::QModelIndex, "QModelIndex"},
    {QMetaType::Void, "void"},
    {QMetaType::QJsonValue, "QJsonValue"},
    {QMetaType::QJsonObject, "QJsonObject"},
    {QMetaType::QJsonArray, "QJsonArray"},
    {QMetaType::QJsonDocument, "QJsonDocument"},
    {QMetaType::QByteArrayList, "QByteArrayList"},
    {QMetaType::QPersistentModelIndex, "QPersistentModelIndex"},
    {QMetaType::Nullptr, "std::nullptr_t"},
    {QMetaType::QCborSimpleType, "QCborSimpleType"},
    {QMetaType::QCborValue, "QCborValue"},
    {QMetaType::QCborArray, "QCborArray"},
    {QMetaType::QCborMap, "QCborMap"},
    {QMetaType::Char16, "char16_t"},
    {QMetaType::Char32, "char32_t"},
    {QMetaType::QVariantPair, "QVariantPair"},
};

// Other spellings of the built-in types.
const BuiltinName kBuiltinAliases[] = {
    {QMetaType::LongLong, "long long"},
    {QMetaType::LongLong, "qint64"},
    {QMetaType::ULongLong, "quint64"},
    {QMetaType::Int, "qint32"},
    {QMetaType::UInt, "quint32"},
    {QMetaType::Short, "qint16"},
    {QMetaType::UShort, "quint16"},
    {QMetaType::SChar, "qint8"},
    {QMetaType::UChar, "quint8"},
    {QMetaType::Double, "qreal"},
    {QMetaType::QVariantList, "QList<QVariant>"},
    {QMetaType::QVariantMap, "QMap<QString,QVariant>"},
    {QMetaType::QVariantHash, "QHash<QString,QVariant>"},
    {QMetaType::QByteArrayList, "QList<QByteArray>"},
    {QMetaType::QStringList, "QList<QString>"},
    {QMetaType::QVariantPair, "std::pair<QVariant,QVariant>"},
    {QMetaType::Nullptr, "nullptr_t"},
    {QMetaType::VoidStar, "void *"},
    {QMetaType::QObjectStar, "QObject *"},
};

const std::array<const QMetaTypeInterface *, kBuiltinCount> &builtinTable() {
    static const std::array<const QMetaTypeInterface *, kBuiltinCount> table = [] {
        std::array<const QMetaTypeInterface *, kBuiltinCount> t{};
#define NT_ENTRY(TYPE)                                                                                                 \
    t[static_cast<std::size_t>(QtPrivate::BuiltinTypeId<TYPE>::value)] = &QtPrivate::MetaTypeInterfaceFor<TYPE>::value;
        NT_ENTRY(bool)
        NT_ENTRY(int)
        NT_ENTRY(unsigned int)
        NT_ENTRY(long long)
        NT_ENTRY(unsigned long long)
        NT_ENTRY(double)
        NT_ENTRY(QChar)
        NT_ENTRY(QString)
        NT_ENTRY(QStringList)
        NT_ENTRY(QByteArray)
        NT_ENTRY(QBitArray)
        NT_ENTRY(QDate)
        NT_ENTRY(QTime)
        NT_ENTRY(QDateTime)
        NT_ENTRY(QUrl)
        NT_ENTRY(QLocale)
        NT_ENTRY(QRegularExpression)
        NT_ENTRY(QEasingCurve)
        NT_ENTRY(QModelIndex)
        NT_ENTRY(QPersistentModelIndex)
        NT_ENTRY(QRect)
        NT_ENTRY(QRectF)
        NT_ENTRY(QSize)
        NT_ENTRY(QSizeF)
        NT_ENTRY(QLine)
        NT_ENTRY(QLineF)
        NT_ENTRY(QPoint)
        NT_ENTRY(QPointF)
        NT_ENTRY(QUuid)
        NT_ENTRY(void *)
        NT_ENTRY(long)
        NT_ENTRY(short)
        NT_ENTRY(char)
        NT_ENTRY(unsigned long)
        NT_ENTRY(unsigned short)
        NT_ENTRY(unsigned char)
        NT_ENTRY(float)
        NT_ENTRY(QObject *)
        NT_ENTRY(signed char)
        NT_ENTRY(QVariant)
        NT_ENTRY(QJsonValue)
        NT_ENTRY(QJsonObject)
        NT_ENTRY(QJsonArray)
        NT_ENTRY(QJsonDocument)
        NT_ENTRY(QByteArrayList)
        NT_ENTRY(std::nullptr_t)
        NT_ENTRY(QCborSimpleType)
        NT_ENTRY(QCborValue)
        NT_ENTRY(QCborArray)
        NT_ENTRY(QCborMap)
        NT_ENTRY(char16_t)
        NT_ENTRY(char32_t)
        NT_ENTRY(QList<QVariant>)
#undef NT_ENTRY
        t[8] = &QtPrivate::MetaTypeInterfaceFor<QMap<QString, QVariant>>::value;
        t[28] = &QtPrivate::MetaTypeInterfaceFor<QHash<QString, QVariant>>::value;
        t[58] = &QtPrivate::MetaTypeInterfaceFor<std::pair<QVariant, QVariant>>::value;
        t[QMetaType::Void] = voidInterface();
        return t;
    }();
    return table;
}

const char *builtinName(int id) {
    for (const BuiltinName &n : kBuiltinNames)
        if (n.id == id)
            return n.name;
    return nullptr;
}

int builtinIdForName(const std::string &name) {
    for (const BuiltinName &n : kBuiltinNames)
        if (name == n.name)
            return n.id;
    for (const BuiltinName &n : kBuiltinAliases)
        if (name == n.name)
            return n.id;
    return 0;
}

int idOf(const QMetaTypeInterface *d) {
    if (!d)
        return 0;
    const int id = d->typeId.load(std::memory_order_acquire);
    return id;
}

int registerInterface(const QMetaTypeInterface *iface, const char *name) {
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (const int id = idOf(iface))
        return id;
    std::string normalized = name ? NtMeta::normalizeType(name) : std::string(iface->nameFn ? iface->nameFn() : "");
    if (normalized.empty())
        return 0;
    if (const int builtin = builtinIdForName(normalized))
        return builtin;
    if (const auto it = r.byName.find(normalized); it != r.byName.end()) {
        iface->typeId.store(it->second, std::memory_order_release);
        return it->second;
    }
    const int id = QMetaType::User + static_cast<int>(r.custom.size());
    r.custom.push_back(iface);
    r.names.push_back(normalized);
    r.byName.emplace(std::move(normalized), id);
    iface->typeId.store(id, std::memory_order_release);
    return id;
}

} // namespace

namespace QtPrivate {

std::string typeNameString(const char *prettyFunction) {
    const std::string s = prettyFunction;
    std::size_t begin = s.find("T = ");
    if (begin == std::string::npos)
        return "unknown";
    begin += 4;
    std::size_t end = s.rfind(']');
    if (end == std::string::npos || end < begin)
        end = s.size();
    std::string name = s.substr(begin, end - begin);
    // GCC appends "; other parameters" after the first template argument.
    if (const std::size_t semi = name.find(';'); semi != std::string::npos && name.find('<') > semi)
        name.erase(semi);
    return NtMeta::normalizeType(name);
}

QByteArray normalizeTypeName(const char *typeName) {
    return QByteArray(NtMeta::normalizeType(typeName ? typeName : "").c_str());
}

int registerNamedType(QMetaType type, const char *name) { return registerInterface(type.iface(), name); }

void *allocateValue(const QMetaTypeInterface *type) {
    const std::size_t size = type->size ? type->size : 1;
    const std::size_t align = type->alignment ? type->alignment : 1;
    void *p =
        align > __STDCPP_DEFAULT_NEW_ALIGNMENT__ ? ::operator new(size, std::align_val_t(align)) : ::operator new(size);
    return p;
}

void freeValue(const QMetaTypeInterface *type, void *where) {
    const std::size_t align = type->alignment ? type->alignment : 1;
    if (align > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        ::operator delete(where, std::align_val_t(align));
    else
        ::operator delete(where);
}

QMetaType::ConverterFunction findRegisteredConverter(QMetaType from, QMetaType to) {
    Registry &r = registry();
    const int a = from.id();
    const int b = to.id();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto it = r.converters.find({a, b});
    return it == r.converters.end() ? QMetaType::ConverterFunction() : it->second;
}

} // namespace QtPrivate

/*!
    Constructs a QMetaType for the built-in or registered type with the id \a typeId. The result
    is invalid if \a typeId is unknown.

    \sa isValid()
*/
QMetaType::QMetaType(int typeId) {
    if (typeId <= 0)
        return;
    if (typeId < User) {
        if (typeId < kBuiltinCount)
            d_ptr = builtinTable()[static_cast<std::size_t>(typeId)];
        return;
    }
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto index = static_cast<std::size_t>(typeId - User);
    if (index < r.custom.size())
        d_ptr = r.custom[index];
}

/*!
    Returns \c true if the type has been registered and has an id; otherwise returns \c false.

    \sa isValid(), id()
*/
bool QMetaType::isRegistered() const noexcept { return d_ptr && idOf(d_ptr) != 0; }

/*!
    Returns the id of the type, registering the type first if it has no id yet. Returns 0 if
    this object is not valid.

    \sa name()
*/
int QMetaType::id() const {
    if (!d_ptr)
        return 0;
    if (const int id = idOf(d_ptr))
        return id;
    if (d_ptr->legacyRegister) {
        d_ptr->legacyRegister();
        if (const int id = idOf(d_ptr))
            return id;
    }
    return registerInterface(d_ptr, nullptr);
}

/*!
    Returns the size in bytes of a value of the type, or 0 if this object is not valid.

    \sa alignOf()
*/
qsizetype QMetaType::sizeOf() const { return d_ptr ? static_cast<qsizetype>(d_ptr->size) : 0; }

/*!
    Returns the alignment in bytes of a value of the type, or 0 if this object is not valid.

    \sa sizeOf()
*/
qsizetype QMetaType::alignOf() const { return d_ptr ? static_cast<qsizetype>(d_ptr->alignment) : 0; }

/*!
    Returns the flags that describe the type.

    \sa QMetaType::TypeFlag
*/
QMetaType::TypeFlags QMetaType::flags() const {
    TypeFlags f;
    if (!d_ptr)
        return f;
    for (const TypeFlag flag :
         {NeedsConstruction, NeedsDestruction, RelocatableType, PointerToQObject, IsEnumeration, IsUnsignedEnumeration})
        if (d_ptr->flags & static_cast<unsigned>(flag))
            f |= flag;
    return f;
}

/*!
    Returns the meta-object of the type if it is a pointer to a QObject-derived class, a class
    with \c Q_GADGET, or an enumeration declared with \c Q_ENUM() or \c Q_FLAG(); otherwise
    returns \c nullptr.

    \sa QMetaObject
*/
const QMetaObject *QMetaType::metaObject() const {
    return d_ptr && d_ptr->metaObjectFn ? d_ptr->metaObjectFn(d_ptr) : nullptr;
}

/*!
    Returns the name of the type, or \c nullptr if this object is not valid. The name of a type
    registered with \c Q_DECLARE_METATYPE() or \c qRegisterMetaType() is the registered name;
    otherwise it is the name as the compiler spells it.

    \sa fromName(), id()
*/
const char *QMetaType::name() const {
    if (!d_ptr)
        return nullptr;
    const int id = idOf(d_ptr);
    if (id > 0 && id < User)
        return builtinName(id);
    if (id >= User) {
        Registry &r = registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        const auto index = static_cast<std::size_t>(id - User);
        if (index < r.names.size())
            return r.names[index].c_str();
    }
    if (d_ptr->name)
        return d_ptr->name;
    return d_ptr->nameFn ? d_ptr->nameFn() : nullptr;
}

/*!
    Constructs a value of the type in the memory at \a where, as a copy of the value at \a copy
    or, if \a copy is \c nullptr, with the default constructor. Returns \a where, or
    \c nullptr if the type cannot be constructed that way.

    \sa create(), destruct()
*/
void *QMetaType::construct(void *where, const void *copy) const {
    if (!d_ptr || !where)
        return nullptr;
    if (copy) {
        if (!d_ptr->copyCtr)
            return nullptr;
        d_ptr->copyCtr(d_ptr, where, copy);
    } else {
        if (!d_ptr->defaultCtr)
            return nullptr;
        d_ptr->defaultCtr(d_ptr, where);
    }
    return where;
}

/*!
    Creates a value of the type on the heap, as a copy of the value at \a copy or, if \a copy is
    \c nullptr, with the default constructor. Returns \c nullptr if the type cannot be
    constructed that way. Free the value with destroy().

    \sa construct(), destroy()
*/
void *QMetaType::create(const void *copy) const {
    if (!d_ptr)
        return nullptr;
    if ((copy && !d_ptr->copyCtr) || (!copy && !d_ptr->defaultCtr))
        return nullptr;
    void *mem = QtPrivate::allocateValue(d_ptr);
    try {
        construct(mem, copy);
    } catch (...) {
        QtPrivate::freeValue(d_ptr, mem);
        throw;
    }
    return mem;
}

/*!
    Destroys the value at \a data without freeing its memory.

    \sa destroy(), construct()
*/
void QMetaType::destruct(void *data) const {
    if (d_ptr && data && d_ptr->dtor)
        d_ptr->dtor(d_ptr, data);
}

/*!
    Destroys the value at \a data, which create() returned, and frees its memory.

    \sa create(), destruct()
*/
void QMetaType::destroy(void *data) const {
    if (!d_ptr || !data)
        return;
    destruct(data);
    QtPrivate::freeValue(d_ptr, data);
}

/*!
    Returns \c true if values of the type can be default-constructed.

    \sa construct()
*/
bool QMetaType::isDefaultConstructible() const noexcept { return d_ptr && d_ptr->defaultCtr; }
/*!
    Returns \c true if values of the type can be copy-constructed.

    \sa construct()
*/
bool QMetaType::isCopyConstructible() const noexcept { return d_ptr && d_ptr->copyCtr; }
/*!
    Returns \c true if values of the type can be move-constructed.

    \sa construct()
*/
bool QMetaType::isMoveConstructible() const noexcept { return d_ptr && d_ptr->moveCtr; }
/*!
    Returns \c true if values of the type can be destroyed.

    \sa destruct()
*/
bool QMetaType::isDestructible() const noexcept { return d_ptr && d_ptr->dtor; }
/*!
    Returns \c true if values of the type can be compared for equality with equals().

    \sa equals()
*/
bool QMetaType::isEqualityComparable() const noexcept { return d_ptr && d_ptr->equals; }
/*!
    Returns \c true if values of the type can be ordered with compare().

    \sa compare()
*/
bool QMetaType::isOrdered() const noexcept { return d_ptr && d_ptr->lessThan && d_ptr->equals; }

/*!
    Returns \c true if the values at \a lhs and \a rhs are equal. A type without \c operator==
    is equal only to itself, that is, when \a lhs and \a rhs are the same address.

    \sa isEqualityComparable(), compare()
*/
bool QMetaType::equals(const void *lhs, const void *rhs) const {
    if (!d_ptr)
        return false;
    if (d_ptr->equals)
        return d_ptr->equals(d_ptr, lhs, rhs);
    return lhs == rhs;
}

/*!
    Compares the values at \a lhs and \a rhs. If the type is ordered, stores a negative number,
    0 or a positive number in \a result when \a lhs is less than, equal to or greater than
    \a rhs, and returns \c true. Otherwise returns \c false.

    \sa isOrdered(), equals()
*/
bool QMetaType::compare(const void *lhs, const void *rhs, int &result) const {
    if (!isOrdered())
        return false;
    if (d_ptr->equals(d_ptr, lhs, rhs))
        result = 0;
    else
        result = d_ptr->lessThan(d_ptr, lhs, rhs) ? -1 : 1;
    return true;
}

/*!
    \overload
    Compares the values at \a lhs and \a rhs and returns the result as an ordering:
    \c QPartialOrdering::Less, \c Equivalent or \c Greater for a type that is ordered, and
    \c QPartialOrdering::Unordered for a type that has no \c operator<, for an invalid type and
    if \a lhs or \a rhs is \c nullptr. Two floating-point values are unordered if one of them
    is not a number.

    \since 6.0
    \sa isOrdered(), equals(), QVariant::compare()
*/
QPartialOrdering QMetaType::compare(const void *lhs, const void *rhs) const {
    if (!d_ptr || !lhs || !rhs || !d_ptr->lessThan)
        return QPartialOrdering::Unordered;
    if (d_ptr->equals && d_ptr->equals(d_ptr, lhs, rhs))
        return QPartialOrdering::Equivalent;
    if (d_ptr->lessThan(d_ptr, lhs, rhs))
        return QPartialOrdering::Less;
    if (d_ptr->lessThan(d_ptr, rhs, lhs))
        return QPartialOrdering::Greater;
    return d_ptr->equals ? QPartialOrdering::Unordered : QPartialOrdering::Equivalent;
}

/*!
    Returns \c true if the type can be written to a QDebug stream.

    \sa debugStream()
*/
bool QMetaType::hasRegisteredDebugStreamOperator() const noexcept { return d_ptr && d_ptr->debugStream; }

/*!
    Writes the value at \a rhs to \a dbg and returns \c true, or returns \c false if the type has
    no debug stream operator.

    \sa hasRegisteredDebugStreamOperator()
*/
bool QMetaType::debugStream(QDebug &dbg, const void *rhs) const {
    if (!d_ptr || !d_ptr->debugStream)
        return false;
    d_ptr->debugStream(d_ptr, dbg, rhs);
    return true;
}

/*!
    Returns \c true if the type can be written to and read from a QDataStream.

    \sa save(), load()
*/
bool QMetaType::hasRegisteredDataStreamOperators() const noexcept {
    return d_ptr && d_ptr->dataStreamOut && d_ptr->dataStreamIn;
}

/*!
    Writes the value at \a data to \a stream and returns \c true, or returns \c false if the type
    cannot be streamed.

    \sa load()
*/
bool QMetaType::save(QDataStream &stream, const void *data) const {
    if (!d_ptr || !d_ptr->dataStreamOut)
        return false;
    d_ptr->dataStreamOut(d_ptr, stream, data);
    return true;
}

/*!
    Reads a value from \a stream into the constructed value at \a data and returns \c true, or
    returns \c false if the type cannot be streamed.

    \sa save()
*/
bool QMetaType::load(QDataStream &stream, void *data) const {
    if (!d_ptr || !d_ptr->dataStreamIn)
        return false;
    d_ptr->dataStreamIn(d_ptr, stream, data);
    return true;
}

bool QMetaType::equalsType(const QMetaType &other) const {
    if (d_ptr == other.d_ptr)
        return true;
    if (!d_ptr || !other.d_ptr)
        return false;
    return id() == other.id();
}

/*!
    Returns \c true if \a type is the id of a built-in or registered type.

    \sa type(), typeName()
*/
bool QMetaType::isRegistered(int type) { return QMetaType(type).isValid(); }

/*!
    Returns the id of the type called \a typeName, or \c UnknownType if there is none.

    \sa fromName(), typeName()
*/
int QMetaType::type(const char *typeName) { return typeName ? fromName(QByteArrayView(typeName)).id() : 0; }

/*!
    \overload
    Returns the id of the type called \a typeName.
*/
int QMetaType::type(const ::QByteArray &typeName) { return fromName(QByteArrayView(typeName)).id(); }

/*!
    Returns the name of the type with the id \a type, or \c nullptr if there is none.

    \sa type(), name()
*/
const char *QMetaType::typeName(int type) {
    if (type > 0 && type < User && !QMetaType(type).isValid())
        return builtinName(type); // a reserved id
    return QMetaType(type).name();
}

/*!
    Returns the QMetaType of the type called \a typeName. The name is normalized first, so
    \c{"unsigned int"} and \c{"uint"} find the same type. The result is invalid if no type is
    registered under the name.

    \sa name()
*/
QMetaType QMetaType::fromName(QByteArrayView typeName) {
    const std::string name =
        NtMeta::normalizeType(std::string(typeName.data(), static_cast<std::size_t>(typeName.size())));
    if (name.empty())
        return QMetaType();
    if (const int builtin = builtinIdForName(name))
        return QMetaType(builtin);
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto it = r.byName.find(name);
    if (it == r.byName.end())
        return QMetaType();
    const auto index = static_cast<std::size_t>(it->second - User);
    return index < r.custom.size() ? QMetaType(r.custom[index]) : QMetaType();
}

/*!
    \overload
    Creates a value of the type with the id \a type, as a copy of \a copy or default-constructed.
*/
void *QMetaType::create(int type, const void *copy) { return QMetaType(type).create(copy); }
/*!
    \overload
    Destroys the value at \a data of the type with the id \a type and frees its memory.
*/
void QMetaType::destroy(int type, void *data) { QMetaType(type).destroy(data); }
/*!
    \overload
    Constructs a value of the type with the id \a type at \a where, as a copy of \a copy or
    default-constructed.
*/
void *QMetaType::construct(int type, void *where, const void *copy) { return QMetaType(type).construct(where, copy); }
/*!
    \overload
    Destroys the value at \a where of the type with the id \a type.
*/
void QMetaType::destruct(int type, void *where) { QMetaType(type).destruct(where); }
/*!
    \overload
    Returns the size of the type with the id \a type.
*/
int QMetaType::sizeOf(int type) { return static_cast<int>(QMetaType(type).sizeOf()); }
/*!
    \overload
    Returns the flags of the type with the id \a type.
*/
QMetaType::TypeFlags QMetaType::typeFlags(int type) { return QMetaType(type).flags(); }
/*!
    \overload
    Returns the meta-object of the type with the id \a type.
*/
const QMetaObject *QMetaType::metaObjectForType(int type) { return QMetaType(type).metaObject(); }
/*!
    \overload
    Returns \c true if the values at \a lhs and \a rhs of the type with the id \a typeId are equal.
*/
bool QMetaType::equals(const void *lhs, const void *rhs, int typeId) { return QMetaType(typeId).equals(lhs, rhs); }
/*!
    \overload
    Writes the value at \a data of the type with the id \a type to \a stream.
*/
bool QMetaType::save(QDataStream &stream, int type, const void *data) { return QMetaType(type).save(stream, data); }
/*!
    \overload
    Reads a value of the type with the id \a type from \a stream into \a data.
*/
bool QMetaType::load(QDataStream &stream, int type, void *data) { return QMetaType(type).load(stream, data); }

/*!
    Makes \a normalizedTypeName another name of \a type, registering the type if needed, and
    returns the id of the type. If the name already belongs to a different type, that type's id
    is returned and nothing changes.

    \sa fromName()
*/
int QMetaType::registerNormalizedTypedef(const ::QByteArray &normalizedTypeName, QMetaType type) {
    const int id = type.id();
    if (id == 0)
        return 0;
    const std::string name = NtMeta::normalizeType(normalizedTypeName.constData());
    if (const int builtin = builtinIdForName(name))
        return builtin;
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    const auto it = r.byName.find(name);
    if (it != r.byName.end())
        return it->second;
    r.byName.emplace(name, id);
    return id;
}

/*!
    Converts the value at \a from of the type \a fromType to the type \a toType, storing the
    result in the constructed value at \a to. Returns \c true on success.

    A converter registered with registerConverter() takes precedence. Otherwise the built-in
    conversions apply:

    \list
    \li Numbers, \c bool and QChar convert to each other. A floating-point number converts to an
        integer by rounding, and fails if it is not finite or does not fit.
    \li QString and QByteArray convert to numbers (an integer, or a number with a fraction
        that is rounded) and to \c bool (false for an empty string, \c{"0"} and \c{"false"}).
    \li Numbers, \c bool, QChar, QDate, QTime, QDateTime, QUrl, QUuid, QVersionNumber and a
        QStringList of one element convert to QString; so does a QByteArray, as UTF-8.
    \li QString converts to QByteArray (as UTF-8), QDate, QTime, QDateTime (ISO 8601), QUrl,
        QUuid, QVersionNumber and QStringList (one element).
    \li QDateTime converts to QDate and QTime, and QDate to QDateTime.
    \li QStringList, QByteArrayList and QVariantList convert to each other when each element
        converts; QVariantMap and QVariantHash convert to each other.
    \li QPoint, QSize, QRect and QLine convert to and from their floating-point variants.
    \li QModelIndex and QPersistentModelIndex convert to each other.
    \li A QJsonValue converts to \c bool, a number, QString, QStringList, QVariantList,
        QVariantMap, QVariantHash, QJsonObject, QJsonArray and QJsonDocument when it holds a
        value of that kind, and all of these, and \c nullptr, convert to a QJsonValue. A
        QJsonObject converts to and from QVariantMap and QVariantHash, a QJsonArray to and from
        QVariantList and QStringList, and a QJsonDocument to and from the object or array it
        holds.
    \li An enumeration converts to and from integers; with \c Q_ENUM() it also converts to and
        from its key name as a QString.
    \li A sequential or associative container converts to QVariantList, QVariantMap or
        QVariantHash when its elements convert.
    \li A pointer to a QObject-derived class converts to another one if the cast succeeds.
    \endlist

    \sa canConvert(), registerConverter(), QVariant::convert()
*/
bool QMetaType::convert(QMetaType fromType, const void *from, QMetaType toType, void *to) {
    if (!fromType.isValid() || !toType.isValid() || !from || !to)
        return false;
    if (fromType == toType) {
        if (!toType.iface()->copyCtr)
            return false;
        // The target is constructed already, so assign through destroy and construct.
        if (from == to)
            return true;
        toType.destruct(to);
        toType.construct(to, from);
        return true;
    }
    if (const ConverterFunction f = QtPrivate::findRegisteredConverter(fromType, toType))
        return f(from, to);
    return QtPrivate::convertBuiltin(fromType, from, toType, to);
}

/*!
    Returns \c true if a value of the type \a fromType can be converted to \a toType with
    convert(). Whether a particular value converts can still depend on its contents.

    \sa convert()
*/
bool QMetaType::canConvert(QMetaType fromType, QMetaType toType) {
    if (!fromType.isValid() || !toType.isValid())
        return false;
    if (fromType == toType)
        return true;
    if (QtPrivate::findRegisteredConverter(fromType, toType))
        return true;
    return QtPrivate::canConvertBuiltin(fromType, toType);
}

/*!
    Registers \a function as the conversion from the type \a from to the type \a to, replacing
    an earlier conversion. Returns \c true.

    \sa convert(), registerConverter()
*/
bool QMetaType::registerConverterFunction(const ConverterFunction &function, QMetaType from, QMetaType to) {
    const int a = from.id();
    const int b = to.id();
    if (!a || !b)
        return false;
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.converters[{a, b}] = function;
    return true;
}

/*!
    Removes the conversion registered from the type \a from to the type \a to, if any.

    \sa registerConverterFunction()
*/
void QMetaType::unregisterConverterFunction(QMetaType from, QMetaType to) {
    const int a = from.id();
    const int b = to.id();
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.converters.erase({a, b});
}
