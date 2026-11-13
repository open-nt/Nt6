// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <new>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

class QDataStream;
class QDebug;
class QObject;
struct QMetaObject;
class QVariant;
class QDate;
class QTime;
class QDateTime;
class QUrl;
class QUuid;
class QVersionNumber;
class QPoint;
class QPointF;
class QSize;
class QSizeF;
class QLine;
class QLineF;
class QRect;
class QRectF;
class QBitArray;
class QLocale;
class QRegularExpression;
class QEasingCurve;
class QModelIndex;
class QPersistentModelIndex;
class QPartialOrdering;
class QJsonValue;
class QJsonObject;
class QCborValue;
class QCborArray;
class QCborMap;
enum class QCborSimpleType : quint8;
class QJsonArray;
class QJsonDocument;
template <typename T> class QSet;
template <typename Enum> class QFlags;

class QMetaType;

namespace QtPrivate {

// Defined with QMetaObject; declared here for the templates below.
const QMetaObject *findMetaObject(const std::type_info &type);

// Operations on a sequential container; see QSequentialIterable.
struct SequenceInterface;
// Operations on an associative container; see QAssociativeIterable.
struct AssociativeInterface;

} // namespace QtPrivate

// The interfaces of QMetaSequence and QMetaAssociation, in <QtCore/qmetacontainer.h>.
namespace QtMetaContainerPrivate {
struct QMetaSequenceInterface;
struct QMetaAssociationInterface;
} // namespace QtMetaContainerPrivate

/*!
    \class QMetaTypeInterface
    \internal
*/
struct QMetaTypeInterface {
    using DefaultCtr = void (*)(const QMetaTypeInterface *, void *);
    using CopyCtr = void (*)(const QMetaTypeInterface *, void *, const void *);
    using MoveCtr = void (*)(const QMetaTypeInterface *, void *, void *);
    using Dtor = void (*)(const QMetaTypeInterface *, void *);
    using Equals = bool (*)(const QMetaTypeInterface *, const void *, const void *);
    using LessThan = bool (*)(const QMetaTypeInterface *, const void *, const void *);
    using DebugStream = void (*)(const QMetaTypeInterface *, QDebug &, const void *);
    using DataStreamOut = void (*)(const QMetaTypeInterface *, QDataStream &, const void *);
    using DataStreamIn = void (*)(const QMetaTypeInterface *, QDataStream &, void *);
    using LegacyRegister = void (*)();
    using MetaObjectFn = const QMetaObject *(*)(const QMetaTypeInterface *);
    using NameFn = const char *(*)();
    using EnumNameFn = const char *(*)();

    mutable std::atomic<int> typeId;
    unsigned short alignment;
    unsigned size;
    unsigned flags;
    const char *name; // may be null; nameFn then computes it
    NameFn nameFn;
    MetaObjectFn metaObjectFn;
    EnumNameFn enumName; // the name Q_ENUM() gave an enumeration, or null
    DefaultCtr defaultCtr;
    CopyCtr copyCtr;
    MoveCtr moveCtr;
    Dtor dtor;
    Equals equals;
    LessThan lessThan;
    DebugStream debugStream;
    DataStreamOut dataStreamOut;
    DataStreamIn dataStreamIn;
    LegacyRegister legacyRegister;
    const QtPrivate::SequenceInterface *sequence;
    const QtPrivate::AssociativeInterface *associative;
};

/*!
    \class QMetaTypeId
    \brief The QMetaTypeId class is the trait that \c Q_DECLARE_METATYPE() specializes.

    \c Q_DECLARE_METATYPE(Type) specializes it for a type so that the type is registered under
    its own name the first time its id is needed. \c Defined is \c 1 for such a type and \c 0
    for every other.

    \differences
    None.
    \enddifferences

    \sa QMetaType
*/
template <typename T> struct QMetaTypeId {
    enum { Defined = 0 };
};

namespace QtPrivate {

// The id QMetaType::Type gives to a built-in type, or 0 for other types.
template <typename T> struct BuiltinTypeId : std::integral_constant<int, 0> {};

#define NT_BUILTIN_METATYPE(TYPE, ID)                                                                                  \
    template <> struct BuiltinTypeId<TYPE> : std::integral_constant<int, ID> {};

NT_BUILTIN_METATYPE(bool, 1)
NT_BUILTIN_METATYPE(int, 2)
NT_BUILTIN_METATYPE(unsigned int, 3)
NT_BUILTIN_METATYPE(long long, 4)
NT_BUILTIN_METATYPE(unsigned long long, 5)
NT_BUILTIN_METATYPE(double, 6)
NT_BUILTIN_METATYPE(QChar, 7)
NT_BUILTIN_METATYPE(QString, 10)
NT_BUILTIN_METATYPE(QStringList, 11)
NT_BUILTIN_METATYPE(QByteArray, 12)
NT_BUILTIN_METATYPE(QBitArray, 13)
NT_BUILTIN_METATYPE(QDate, 14)
NT_BUILTIN_METATYPE(QTime, 15)
NT_BUILTIN_METATYPE(QDateTime, 16)
NT_BUILTIN_METATYPE(QUrl, 17)
NT_BUILTIN_METATYPE(QLocale, 18)
NT_BUILTIN_METATYPE(QRect, 19)
NT_BUILTIN_METATYPE(QRectF, 20)
NT_BUILTIN_METATYPE(QSize, 21)
NT_BUILTIN_METATYPE(QSizeF, 22)
NT_BUILTIN_METATYPE(QLine, 23)
NT_BUILTIN_METATYPE(QLineF, 24)
NT_BUILTIN_METATYPE(QPoint, 25)
NT_BUILTIN_METATYPE(QPointF, 26)
NT_BUILTIN_METATYPE(QRegularExpression, 27)
NT_BUILTIN_METATYPE(QEasingCurve, 29)
NT_BUILTIN_METATYPE(QUuid, 30)
NT_BUILTIN_METATYPE(void *, 31)
NT_BUILTIN_METATYPE(long, 32)
NT_BUILTIN_METATYPE(short, 33)
NT_BUILTIN_METATYPE(char, 34)
NT_BUILTIN_METATYPE(unsigned long, 35)
NT_BUILTIN_METATYPE(unsigned short, 36)
NT_BUILTIN_METATYPE(unsigned char, 37)
NT_BUILTIN_METATYPE(float, 38)
NT_BUILTIN_METATYPE(QObject *, 39)
NT_BUILTIN_METATYPE(signed char, 40)
NT_BUILTIN_METATYPE(QVariant, 41)
NT_BUILTIN_METATYPE(QModelIndex, 42)
NT_BUILTIN_METATYPE(void, 43)
NT_BUILTIN_METATYPE(QJsonValue, 45)
NT_BUILTIN_METATYPE(QJsonObject, 46)
NT_BUILTIN_METATYPE(QJsonArray, 47)
NT_BUILTIN_METATYPE(QJsonDocument, 48)
NT_BUILTIN_METATYPE(QByteArrayList, 49)
NT_BUILTIN_METATYPE(QPersistentModelIndex, 50)
NT_BUILTIN_METATYPE(std::nullptr_t, 51)
NT_BUILTIN_METATYPE(QCborSimpleType, 52)
NT_BUILTIN_METATYPE(QCborValue, 53)
NT_BUILTIN_METATYPE(QCborArray, 54)
NT_BUILTIN_METATYPE(QCborMap, 55)
NT_BUILTIN_METATYPE(char16_t, 56)
NT_BUILTIN_METATYPE(char32_t, 57)
NT_BUILTIN_METATYPE(QList<QVariant>, 9)
#undef NT_BUILTIN_METATYPE
template <> struct BuiltinTypeId<QMap<QString, QVariant>> : std::integral_constant<int, 8> {};
template <> struct BuiltinTypeId<QHash<QString, QVariant>> : std::integral_constant<int, 28> {};
template <> struct BuiltinTypeId<std::pair<QVariant, QVariant>> : std::integral_constant<int, 58> {};

template <typename T, typename = void> struct IsComplete : std::false_type {};
template <typename T> struct IsComplete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

template <typename T, typename = void> struct HasEqual : std::false_type {};
template <typename T>
struct HasEqual<T, std::void_t<decltype(static_cast<bool>(std::declval<const T &>() == std::declval<const T &>()))>>
    : std::true_type {};
template <typename T> struct HasEqual<QList<T>> : HasEqual<T> {};
template <typename T> struct HasEqual<QSet<T>> : HasEqual<T> {};
template <typename T> struct HasEqual<std::vector<T>> : HasEqual<T> {};
template <typename T> struct HasEqual<std::list<T>> : HasEqual<T> {};
template <typename T> struct HasEqual<std::deque<T>> : HasEqual<T> {};
template <typename K, typename V> struct HasEqual<QMap<K, V>> : std::conjunction<HasEqual<K>, HasEqual<V>> {};
template <typename K, typename V> struct HasEqual<QHash<K, V>> : std::conjunction<HasEqual<K>, HasEqual<V>> {};
template <typename K, typename V> struct HasEqual<std::map<K, V>> : std::conjunction<HasEqual<K>, HasEqual<V>> {};
template <typename K, typename V>
struct HasEqual<std::unordered_map<K, V>> : std::conjunction<HasEqual<K>, HasEqual<V>> {};
template <typename A, typename B> struct HasEqual<std::pair<A, B>> : std::conjunction<HasEqual<A>, HasEqual<B>> {};

template <typename T> bool metaNotEqual(const T &a, const T &b) {
    if constexpr (HasEqual<T>::value)
        return !(a == b);
    else
        return true;
}

template <typename T, typename = void> struct HasLess : std::false_type {};
template <typename T>
struct HasLess<T, std::void_t<decltype(static_cast<bool>(std::declval<const T &>() < std::declval<const T &>()))>>
    : std::true_type {};
template <typename T> struct HasLess<QList<T>> : HasLess<T> {};
template <typename T> struct HasLess<std::vector<T>> : HasLess<T> {};
template <typename T> struct HasLess<std::list<T>> : HasLess<T> {};
template <typename T> struct HasLess<std::deque<T>> : HasLess<T> {};
template <typename K, typename V> struct HasLess<QMap<K, V>> : std::false_type {};
template <typename K, typename V> struct HasLess<QHash<K, V>> : std::false_type {};
template <typename K, typename V> struct HasLess<std::map<K, V>> : std::false_type {};
template <typename K, typename V> struct HasLess<std::unordered_map<K, V>> : std::false_type {};
template <typename A, typename B> struct HasLess<std::pair<A, B>> : std::conjunction<HasLess<A>, HasLess<B>> {};
template <typename T> struct HasLess<QSet<T>> : std::false_type {};

// Streaming needs the stream classes to be complete where the type is first used.
template <typename T, typename = void> struct HasDebug : std::false_type {};
template <typename T>
struct HasDebug<T, std::void_t<decltype(std::declval<QDebug &>() << std::declval<const T &>())>> : std::true_type {};
template <typename T> struct HasDebug<QList<T>> : HasDebug<T> {};
template <typename T> struct HasDebug<QSet<T>> : std::false_type {};
template <typename K, typename V> struct HasDebug<QMap<K, V>> : std::conjunction<HasDebug<K>, HasDebug<V>> {};
template <typename K, typename V> struct HasDebug<QHash<K, V>> : std::conjunction<HasDebug<K>, HasDebug<V>> {};
template <typename T> struct HasDebug<std::vector<T>> : std::false_type {};

template <typename T, typename = void> struct HasStreamOut : std::false_type {};
template <typename T>
struct HasStreamOut<T, std::void_t<decltype(std::declval<QDataStream &>() << std::declval<const T &>())>>
    : std::true_type {};
template <typename T, typename = void> struct HasStreamIn : std::false_type {};
template <typename T>
struct HasStreamIn<T, std::void_t<decltype(std::declval<QDataStream &>() >> std::declval<T &>())>> : std::true_type {};
template <typename T> struct HasStreamOut<QList<T>> : HasStreamOut<T> {};
template <typename T> struct HasStreamIn<QList<T>> : HasStreamIn<T> {};
template <typename K, typename V>
struct HasStreamOut<QMap<K, V>> : std::conjunction<HasStreamOut<K>, HasStreamOut<V>> {};
template <typename K, typename V> struct HasStreamIn<QMap<K, V>> : std::conjunction<HasStreamIn<K>, HasStreamIn<V>> {};
template <typename K, typename V>
struct HasStreamOut<QHash<K, V>> : std::conjunction<HasStreamOut<K>, HasStreamOut<V>> {};
template <typename K, typename V> struct HasStreamIn<QHash<K, V>> : std::conjunction<HasStreamIn<K>, HasStreamIn<V>> {};
template <typename T> struct HasStreamOut<QSet<T>> : std::false_type {};
template <typename T> struct HasStreamIn<QSet<T>> : std::false_type {};
template <typename A, typename B> struct HasStreamOut<std::pair<A, B>> : std::false_type {};
template <typename A, typename B> struct HasStreamIn<std::pair<A, B>> : std::false_type {};

template <typename T, typename = void> struct HasStaticMetaObject : std::false_type {};
template <typename T> struct HasStaticMetaObject<T, std::void_t<decltype(T::staticMetaObject)>> : std::true_type {};

template <typename T, typename = void> struct EnumMetaObject {
    static const QMetaObject *get() { return nullptr; }
};
// Q_ENUM() and Q_FLAG() define qt_getEnumMetaObject() as a friend of the class.
template <typename T> struct EnumMetaObject<T, std::void_t<decltype(qt_getEnumMetaObject(std::declval<T>()))>> {
    static const QMetaObject *get() { return qt_getEnumMetaObject(T()); }
};

template <typename T, typename = void> struct EnumNameOf {
    static constexpr QMetaTypeInterface::EnumNameFn get() { return nullptr; }
};
template <typename T> struct EnumNameOf<T, std::void_t<decltype(qt_getEnumName(std::declval<T>()))>> {
    static const char *name() { return qt_getEnumName(T()); }
    static constexpr QMetaTypeInterface::EnumNameFn get() { return &name; }
};

template <typename T> struct IsQFlags : std::false_type {};
template <typename E> struct IsQFlags<QFlags<E>> : std::true_type {};

// Name of T as the compiler spells it, normalized.
std::string typeNameString(const char *prettyFunction);
QByteArray normalizeTypeName(const char *typeName);

template <typename T> const char *typeNameOf() {
    static const std::string name = typeNameString(
#if defined(__GNUC__) || defined(__clang__)
        __PRETTY_FUNCTION__
#else
        __FUNCSIG__
#endif
    );
    return name.c_str();
}

constexpr unsigned kFlagNeedsConstruction = 0x1;
constexpr unsigned kFlagNeedsDestruction = 0x2;
constexpr unsigned kFlagRelocatable = 0x4;
constexpr unsigned kFlagPointerToQObject = 0x8;
constexpr unsigned kFlagIsEnumeration = 0x10;
constexpr unsigned kFlagIsUnsignedEnumeration = 0x100;
constexpr unsigned kFlagIsPointer = 0x10000;

template <typename T> struct MetaTypeOps {
    static void defaultCtr(const QMetaTypeInterface *, void *where) { new (where) T(); }
    static void copyCtr(const QMetaTypeInterface *, void *where, const void *src) {
        new (where) T(*static_cast<const T *>(src));
    }
    static void moveCtr(const QMetaTypeInterface *, void *where, void *src) {
        new (where) T(std::move(*static_cast<T *>(src)));
    }
    static void dtor(const QMetaTypeInterface *, void *p) { static_cast<T *>(p)->~T(); }
    static bool equals(const QMetaTypeInterface *, const void *a, const void *b) {
        return static_cast<bool>(*static_cast<const T *>(a) == *static_cast<const T *>(b));
    }
    static bool lessThan(const QMetaTypeInterface *, const void *a, const void *b) {
        return static_cast<bool>(*static_cast<const T *>(a) < *static_cast<const T *>(b));
    }
    static void debugStream(const QMetaTypeInterface *, QDebug &d, const void *p) { d << *static_cast<const T *>(p); }
    static void dataStreamOut(const QMetaTypeInterface *, QDataStream &s, const void *p) {
        s << *static_cast<const T *>(p);
    }
    static void dataStreamIn(const QMetaTypeInterface *, QDataStream &s, void *p) { s >> *static_cast<T *>(p); }
    static void legacyRegister();
    static const char *name() { return typeNameOf<T>(); }
    static const QMetaObject *metaObject(const QMetaTypeInterface *);
};

template <typename T> constexpr QMetaTypeInterface::DefaultCtr defaultCtrOf() {
    if constexpr (std::is_default_constructible_v<T>)
        return &MetaTypeOps<T>::defaultCtr;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::CopyCtr copyCtrOf() {
    if constexpr (std::is_copy_constructible_v<T>)
        return &MetaTypeOps<T>::copyCtr;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::MoveCtr moveCtrOf() {
    if constexpr (std::is_move_constructible_v<T>)
        return &MetaTypeOps<T>::moveCtr;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::Equals equalsOf() {
    if constexpr (HasEqual<T>::value)
        return &MetaTypeOps<T>::equals;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::LessThan lessThanOf() {
    if constexpr (HasLess<T>::value && !std::is_function_v<std::remove_pointer_t<T>>)
        return &MetaTypeOps<T>::lessThan;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::DebugStream debugOf() {
    if constexpr (HasDebug<T>::value)
        return &MetaTypeOps<T>::debugStream;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::DataStreamOut streamOutOf() {
    if constexpr (HasStreamOut<T>::value && HasStreamIn<T>::value)
        return &MetaTypeOps<T>::dataStreamOut;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::DataStreamIn streamInOf() {
    if constexpr (HasStreamOut<T>::value && HasStreamIn<T>::value)
        return &MetaTypeOps<T>::dataStreamIn;
    else
        return nullptr;
}
template <typename T> constexpr QMetaTypeInterface::LegacyRegister legacyOf() {
    if constexpr (QMetaTypeId<T>::Defined)
        return &MetaTypeOps<T>::legacyRegister;
    else
        return nullptr;
}

template <typename T, bool = std::is_enum_v<T>> struct IsUnsignedEnum : std::false_type {};
template <typename T>
struct IsUnsignedEnum<T, true> : std::bool_constant<!std::is_signed_v<std::underlying_type_t<T>>> {};

template <typename T> constexpr unsigned flagsOf() {
    unsigned f = 0;
    if (!std::is_trivially_default_constructible_v<T>)
        f |= kFlagNeedsConstruction;
    if (!std::is_trivially_destructible_v<T>)
        f |= kFlagNeedsDestruction;
    if (std::is_trivially_copyable_v<T>)
        f |= kFlagRelocatable;
    if (std::is_enum_v<T> || IsQFlags<T>::value)
        f |= kFlagIsEnumeration;
    if (IsUnsignedEnum<T>::value)
        f |= kFlagIsUnsignedEnumeration;
    if (std::is_pointer_v<T>)
        f |= kFlagIsPointer;
    return f;
}

template <typename T> struct IsPointerToQObject : std::false_type {};
template <typename T> struct IsPointerToQObject<T *> : std::conjunction<IsComplete<T>, std::is_base_of<QObject, T>> {};

template <typename T> struct SequenceInterfaceFor;
template <typename T> struct AssociativeInterfaceFor;

template <typename T> constexpr const SequenceInterface *sequenceOf();
template <typename T> constexpr const AssociativeInterface *associativeOf();

template <typename T> constexpr QMetaTypeInterface makeMetaTypeInterface() {
    return QMetaTypeInterface{{BuiltinTypeId<T>::value},
                              static_cast<unsigned short>(alignof(T)),
                              static_cast<unsigned>(sizeof(T)),
                              flagsOf<T>() | (IsPointerToQObject<T>::value ? kFlagPointerToQObject : 0u),
                              nullptr,
                              &MetaTypeOps<T>::name,
                              &MetaTypeOps<T>::metaObject,
                              EnumNameOf<T>::get(),
                              defaultCtrOf<T>(),
                              copyCtrOf<T>(),
                              moveCtrOf<T>(),
                              &MetaTypeOps<T>::dtor,
                              equalsOf<T>(),
                              lessThanOf<T>(),
                              debugOf<T>(),
                              streamOutOf<T>(),
                              streamInOf<T>(),
                              legacyOf<T>(),
                              sequenceOf<T>(),
                              associativeOf<T>()};
}

template <typename T> struct MetaTypeInterfaceFor {
    static inline constexpr QMetaTypeInterface value = makeMetaTypeInterface<T>();
};

} // namespace QtPrivate

/*!
    \class QMetaType
    \brief The QMetaType class manages named types in the meta-object system.

    QMetaType describes a C++ type at run time: its name, size, how to
    construct, copy, compare and destroy a value, and how to stream it.
    QVariant, queued connections and the property system are built on it.

    Obtain one for a type with fromType(), or from an id or a name:

    \code
    QMetaType t = QMetaType::fromType<QString>();
    void *s = t.create();      // a new, empty QString
    t.destroy(s);

    QMetaType u = QMetaType::fromName("QString");
    Q_ASSERT(t == u && t.id() == QMetaType::QString);
    \endcode

    The built-in types have the same ids as in Qt. Any other type gets an id
    the first time it is registered, with \c qRegisterMetaType(),
    \c Q_DECLARE_METATYPE() or by asking for its id(); ids of types registered
    at run time are at least \c QMetaType::User.

    Debug streaming (QDebug) and data streaming (QDataStream) of a type are
    available when its \c operator<< and \c operator>> exist and the stream
    classes are declared where the type is first used. Equality and ordering
    are detected the same way.

    \differences
    The layout of QMetaTypeInterface is Nt's own. Enumerators of
    \c QMetaType::Type for classes that Nt does not have (the CBOR
    classes) are reserved, but no type is registered for them.
    \c QMetaType::QVersionNumber
    does not exist; QVersionNumber, QMargins and QMarginsF get an id at the
    first registration like any user type. There is no
    \c registerComparators() or support for smart-pointer metatypes. unregisterMetaType() forgets the names of a
    type but keeps its id reserved. A type with
    no \c operator== compares equal in a QVariant only when both variants
    share the same stored value.
    \enddifferences

    \sa QVariant, QMetaTypeId
*/
class QMetaType {
public:
    /*!
        \enum QMetaType::Type

        The identifiers of the types that are built in. A type registered at
        run time gets an id from \c User upward.

        \value UnknownType No type.
        \value Bool \c bool.
        \value Int \c int.
        \value UInt \c{unsigned int}.
        \value LongLong \c{long long}.
        \value ULongLong \c{unsigned long long}.
        \value Double \c double.
        \value Long \c long.
        \value Short \c short.
        \value Char \c char.
        \value Char16 \c char16_t.
        \value Char32 \c char32_t.
        \value ULong \c{unsigned long}.
        \value UShort \c{unsigned short}.
        \value UChar \c{unsigned char}.
        \value Float \c float.
        \value SChar \c{signed char}.
        \value Nullptr \c std::nullptr_t.
        \value VoidStar \c{void *}.
        \value QObjectStar \c{QObject *}.
        \value Void \c void.
        \value QChar QChar.
        \value QString QString.
        \value QStringList QStringList.
        \value QByteArray QByteArray.
        \value QByteArrayList QByteArrayList.
        \value QBitArray QBitArray.
        \value QDate QDate.
        \value QTime QTime.
        \value QDateTime QDateTime.
        \value QUrl QUrl.
        \value QLocale QLocale.
        \value QRect QRect.
        \value QRectF QRectF.
        \value QSize QSize.
        \value QSizeF QSizeF.
        \value QLine QLine.
        \value QLineF QLineF.
        \value QPoint QPoint.
        \value QPointF QPointF.
        \value QRegularExpression QRegularExpression.
        \value QEasingCurve QEasingCurve.
        \value QUuid QUuid.
        \value QVariant QVariant.
        \value QModelIndex QModelIndex.
        \value QPersistentModelIndex QPersistentModelIndex.
        \value QJsonValue QJsonValue.
        \value QJsonObject QJsonObject.
        \value QJsonArray QJsonArray.
        \value QJsonDocument QJsonDocument.
        \value QCborSimpleType QCborSimpleType.
        \value QCborValue QCborValue.
        \value QCborArray QCborArray.
        \value QCborMap QCborMap.
        \value QVariantMap \c{QMap<QString, QVariant>}.
        \value QVariantList \c{QList<QVariant>}.
        \value QVariantHash \c{QHash<QString, QVariant>}.
        \value QVariantPair \c{std::pair<QVariant, QVariant>}.
        \value User The first id of a type registered at run time.
    */
    enum Type {
        UnknownType = 0,
        Bool = 1,
        Int = 2,
        UInt = 3,
        LongLong = 4,
        ULongLong = 5,
        Double = 6,
        QChar = 7,
        QVariantMap = 8,
        QVariantList = 9,
        QString = 10,
        QStringList = 11,
        QByteArray = 12,
        QBitArray = 13,
        QDate = 14,
        QTime = 15,
        QDateTime = 16,
        QUrl = 17,
        QLocale = 18,
        QRect = 19,
        QRectF = 20,
        QSize = 21,
        QSizeF = 22,
        QLine = 23,
        QLineF = 24,
        QPoint = 25,
        QPointF = 26,
        QRegularExpression = 27,
        QVariantHash = 28,
        QEasingCurve = 29,
        QUuid = 30,
        VoidStar = 31,
        Long = 32,
        Short = 33,
        Char = 34,
        ULong = 35,
        UShort = 36,
        UChar = 37,
        Float = 38,
        QObjectStar = 39,
        SChar = 40,
        QVariant = 41,
        QModelIndex = 42,
        Void = 43,
        QJsonValue = 45,
        QJsonObject = 46,
        QJsonArray = 47,
        QJsonDocument = 48,
        QByteArrayList = 49,
        QPersistentModelIndex = 50,
        Nullptr = 51,
        QCborSimpleType = 52,
        QCborValue = 53,
        QCborArray = 54,
        QCborMap = 55,
        Char16 = 56,
        Char32 = 57,
        QVariantPair = 58,
        User = 65536
    };

    /*!
        \enum QMetaType::TypeFlag

        These flags describe a type.

        \value NeedsConstruction Values of the type must be constructed.
        \value NeedsDestruction Values of the type must be destroyed.
        \value RelocatableType Values can be moved in memory with \c memcpy.
        \value PointerToQObject The type is a pointer to a QObject-derived class.
        \value IsEnumeration The type is an enumeration.
        \value IsUnsignedEnumeration The type is an enumeration with an unsigned underlying type.
    */
    enum TypeFlag {
        NeedsConstruction = 0x1,
        NeedsDestruction = 0x2,
        RelocatableType = 0x4,
        PointerToQObject = 0x8,
        IsEnumeration = 0x10,
        IsUnsignedEnumeration = 0x100
    };
    Q_DECLARE_FLAGS(TypeFlags, TypeFlag)

    QMetaType(int typeId = UnknownType);
    /*!
        Constructs a QMetaType from the interface \a d. This is meant for
        code that Nt generates.
    */
    explicit QMetaType(const QMetaTypeInterface *d) noexcept : d_ptr(d) {}

    /*!
        Returns \c true if this object describes a type; otherwise returns
        \c false.

        \sa isRegistered()
    */
    bool isValid() const noexcept { return d_ptr != nullptr; }
    bool isRegistered() const noexcept;
    int id() const;
    qsizetype sizeOf() const;
    qsizetype alignOf() const;
    TypeFlags flags() const;
    const QMetaObject *metaObject() const;
    const char *name() const;

    void *construct(void *where, const void *copy = nullptr) const;
    void *create(const void *copy = nullptr) const;
    void destruct(void *data) const;
    void destroy(void *data) const;

    bool isDefaultConstructible() const noexcept;
    bool isCopyConstructible() const noexcept;
    bool isMoveConstructible() const noexcept;
    bool isDestructible() const noexcept;
    bool isEqualityComparable() const noexcept;
    bool isOrdered() const noexcept;

    bool equals(const void *lhs, const void *rhs) const;
    bool compare(const void *lhs, const void *rhs, int &result) const;
    QPartialOrdering compare(const void *lhs, const void *rhs) const;

    bool hasRegisteredDebugStreamOperator() const noexcept;
    bool debugStream(QDebug &dbg, const void *rhs) const;
    bool hasRegisteredDataStreamOperators() const noexcept;
    bool save(QDataStream &stream, const void *data) const;
    bool load(QDataStream &stream, void *data) const;

    /*!
        Returns \c true if \a a and \a b describe the same type.
    */
    friend bool operator==(const QMetaType &a, const QMetaType &b) { return a.equalsType(b); }
    /*!
        Returns \c true if \a a and \a b describe different types.
    */
    friend bool operator!=(const QMetaType &a, const QMetaType &b) { return !a.equalsType(b); }

    static bool isRegistered(int type);
    static int type(const char *typeName);
    static int type(const ::QByteArray &typeName);
    static const char *typeName(int type);
    static QMetaType fromName(QByteArrayView typeName);
    static void *create(int type, const void *copy = nullptr);
    static void destroy(int type, void *data);
    static void *construct(int type, void *where, const void *copy);
    static void destruct(int type, void *where);
    static int sizeOf(int type);
    static TypeFlags typeFlags(int type);
    static const QMetaObject *metaObjectForType(int type);
    static bool equals(const void *lhs, const void *rhs, int typeId);
    static bool save(QDataStream &stream, int type, const void *data);
    static bool load(QDataStream &stream, int type, void *data);
    static int registerNormalizedTypedef(const ::QByteArray &normalizedTypeName, QMetaType type);

    /*!
        The type of a function that converts a value. It reads the source value at its first
        argument and stores the result in the already constructed target at its second argument.
        It returns \c false if the conversion fails.
    */
    using ConverterFunction = std::function<bool(const void *, void *)>;

    static bool convert(QMetaType fromType, const void *from, QMetaType toType, void *to);
    static bool canConvert(QMetaType fromType, QMetaType toType);
    static bool registerConverterFunction(const ConverterFunction &function, QMetaType from, QMetaType to);
    static void unregisterConverterFunction(QMetaType from, QMetaType to);
    static bool hasRegisteredConverterFunction(QMetaType fromType, QMetaType toType);
    /*!
        \since 5.2
        \overload
        Returns \c true if a conversion from \c From to \c To was registered; otherwise returns \c false.
    */
    template <typename From, typename To> static bool hasRegisteredConverterFunction() {
        return hasRegisteredConverterFunction(fromType<From>(), fromType<To>());
    }

    /*!
        \since 6.0
        The type of a function that makes a view of a value: it receives the value at its first argument, which it
        may change, and fills the already constructed view at its second argument. It returns \c false if it
        cannot.
    */
    using MutableViewFunction = std::function<bool(void *, void *)>;

    static bool view(QMetaType fromType, void *from, QMetaType toType, void *to);
    static bool canView(QMetaType fromType, QMetaType toType);
    static bool registerMutableViewFunction(const MutableViewFunction &function, QMetaType from, QMetaType to);
    static void unregisterMutableViewFunction(QMetaType from, QMetaType to);
    static bool hasRegisteredMutableViewFunction(QMetaType fromType, QMetaType toType);
    /*!
        \since 6.0
        \overload
        Returns \c true if a view from \c From to \c To was registered; otherwise returns \c false.
    */
    template <typename From, typename To> static bool hasRegisteredMutableViewFunction() {
        return hasRegisteredMutableViewFunction(fromType<From>(), fromType<To>());
    }
    /*!
        \since 6.0
        Registers the member function \a function of \c From, which returns a view of the value as a \c To, as the
        view from \c From to \c To. Returns \c true if the registration succeeded.
    */
    template <typename From, typename To> static bool registerMutableView(To (From::*function)()) {
        return registerMutableViewFunction(
            [function](void *src, void *dst) -> bool {
                *static_cast<std::remove_cv_t<std::remove_reference_t<To>> *>(dst) =
                    (static_cast<From *>(src)->*function)();
                return true;
            },
            fromType<From>(), fromType<To>());
    }
    /*!
        \since 6.0
        \overload
        Registers \a function, a callable that takes a \c{From &} and returns a \c To, as the view.
    */
    template <typename From, typename To, typename UnaryFunction>
    static bool registerMutableView(UnaryFunction function) {
        return registerMutableViewFunction(
            [function](void *src, void *dst) -> bool {
                *static_cast<To *>(dst) = function(*static_cast<From *>(src));
                return true;
            },
            fromType<From>(), fromType<To>());
    }

    void registerType() const;
    QMetaType underlyingType() const;
    static void unregisterMetaType(QMetaType type);

    /*!
        Registers the implicit conversion from \c From to \c To, so that a QVariant that holds a
        \c From converts to a \c To. Returns \c true if the registration succeeded.
    */
    template <typename From, typename To> static bool registerConverter() {
        static_assert(std::is_convertible_v<From, To>, "From must be implicitly convertible to To");
        return registerConverter<From, To>([](const From &from) -> To { return To(from); });
    }
    /*!
        \overload
        Registers the member function \a function of \c From as the conversion to \c To.
    */
    template <typename From, typename To> static bool registerConverter(To (From::*function)() const) {
        return registerConverter<From, To>([function](const From &from) -> To { return (from.*function)(); });
    }
    /*!
        \overload
        Registers the member function \a function of \c From, which reports success through its
        \c bool pointer argument, as the conversion to \c To.
    */
    template <typename From, typename To> static bool registerConverter(To (From::*function)(bool *) const) {
        return registerConverterFunction(
            [function](const void *src, void *dst) -> bool {
                bool ok = false;
                *static_cast<To *>(dst) = (static_cast<const From *>(src)->*function)(&ok);
                return ok;
            },
            fromType<From>(), fromType<To>());
    }
    /*!
        \overload
        Registers \a function, a callable that takes a \c{const From &} and returns a \c To, as
        the conversion.
    */
    template <typename From, typename To, typename UnaryFunction>
    static bool registerConverter(UnaryFunction function) {
        return registerConverterFunction(
            [function](const void *src, void *dst) -> bool {
                *static_cast<To *>(dst) = function(*static_cast<const From *>(src));
                return true;
            },
            fromType<From>(), fromType<To>());
    }

    /*!
        Returns the QMetaType of \c T. The qualifiers of \c T are ignored.
    */
    template <typename T> static QMetaType fromType() {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        constexpr int builtin = QtPrivate::BuiltinTypeId<U>::value;
        if constexpr (builtin != 0)
            return QMetaType(builtin);
        else
            return QMetaType(&QtPrivate::MetaTypeInterfaceFor<U>::value);
    }

    /*!
        Returns the interface that describes the type. This is meant for code that Nt generates.
    */
    const QMetaTypeInterface *iface() const noexcept { return d_ptr; }

private:
    bool equalsType(const QMetaType &other) const;

    const QMetaTypeInterface *d_ptr = nullptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QMetaType::TypeFlags)

namespace QtPrivate {
int registerNamedType(QMetaType type, const char *name);
} // namespace QtPrivate

// Q_DECLARE_METATYPE() gives a type a name that qRegisterMetaType() and fromName() use.
#define Q_DECLARE_METATYPE(...)                                                                                        \
    template <> struct QMetaTypeId<__VA_ARGS__> {                                                                      \
        enum { Defined = 1 };                                                                                          \
        static int qt_metatype_id() {                                                                                  \
            return QtPrivate::registerNamedType(QMetaType::fromType<__VA_ARGS__>(), #__VA_ARGS__);                     \
        }                                                                                                              \
    };

template <typename T> int qRegisterMetaType();
template <typename T> int qRegisterMetaType(const char *typeName);

namespace QtPrivate {

template <typename T> void MetaTypeOps<T>::legacyRegister() { QMetaTypeId<T>::qt_metatype_id(); }

template <typename T> const QMetaObject *MetaTypeOps<T>::metaObject(const QMetaTypeInterface *) {
    if constexpr (std::is_enum_v<T> || IsQFlags<T>::value) {
        return EnumMetaObject<T>::get();
    } else if constexpr (std::is_class_v<T> && IsComplete<T>::value && HasStaticMetaObject<T>::value) {
        return findMetaObject(typeid(T));
    } else if constexpr (IsPointerToQObject<T>::value) {
        using Pointee = std::remove_pointer_t<T>;
        if constexpr (HasStaticMetaObject<Pointee>::value)
            return findMetaObject(typeid(Pointee));
        else
            return nullptr;
    } else {
        return nullptr;
    }
}

} // namespace QtPrivate

// Registers the type T with the meta-type system, if it is not registered yet, and returns its id.

template <typename T> int qRegisterMetaType() { return QMetaType::fromType<T>().id(); }

// Registers the type T under the additional name typeName, and returns its id. The name can then
// be used with QMetaType::fromName() and in property and method signatures.
template <typename T> int qRegisterMetaType(const char *typeName) {
    const QMetaType type = QMetaType::fromType<T>();
    const int id = type.id();
    QMetaType::registerNormalizedTypedef(QtPrivate::normalizeTypeName(typeName), type);
    return id;
}

// Returns the meta-type id of T, registering the type if needed.
template <typename T> int qMetaTypeId() { return QMetaType::fromType<T>().id(); }

namespace QtPrivate {

struct SequenceInterface {
    const QMetaTypeInterface *(*valueType)();
    qsizetype (*size)(const void *);
    void (*valueAt)(const void *, qsizetype, void *); // copy-constructs the element at the second argument
    bool (*append)(void *, const void *);             // null if the container cannot grow
    void (*clear)(void *);                            // null if the container cannot be cleared
    const QtMetaContainerPrivate::QMetaSequenceInterface *(*metaSequence)(); // defined in qmetacontainer.h
};

struct AssociativeInterface {
    const QMetaTypeInterface *(*keyType)();
    const QMetaTypeInterface *(*mappedType)();
    qsizetype (*size)(const void *);
    void (*keyAt)(const void *, qsizetype, void *);    // copy-constructs the key at the third argument
    void (*mappedAt)(const void *, qsizetype, void *); // copy-constructs the mapped value there
    qsizetype (*find)(const void *, const void *);     // position of a key, or -1
    bool (*insert)(void *, const void *, const void *);
    void (*clear)(void *);
    const QtMetaContainerPrivate::QMetaAssociationInterface *(*metaAssociation)(); // defined in qmetacontainer.h
};

template <typename C> struct SequenceTraits {
    static constexpr bool value = false;
};
template <typename V> struct SequenceTraits<QList<V>> {
    static constexpr bool value = true;
    using Value = V;
};
template <typename V> struct SequenceTraits<QQueue<V>> {
    static constexpr bool value = true;
    using Value = V;
};
template <typename V> struct SequenceTraits<QStack<V>> {
    static constexpr bool value = true;
    using Value = V;
};
template <> struct SequenceTraits<QStringList> {
    static constexpr bool value = true;
    using Value = QString;
};
template <typename V> struct SequenceTraits<QSet<V>> {
    static constexpr bool value = true;
    using Value = V;
};
template <typename V, typename A> struct SequenceTraits<std::vector<V, A>> {
    static constexpr bool value = true;
    using Value = V;
};
template <typename V, typename A> struct SequenceTraits<std::list<V, A>> {
    static constexpr bool value = true;
    using Value = V;
};
template <typename V, typename A> struct SequenceTraits<std::deque<V, A>> {
    static constexpr bool value = true;
    using Value = V;
};

template <typename C> struct AssociativeTraits {
    static constexpr bool value = false;
};
template <typename K, typename V> struct AssociativeTraits<QMap<K, V>> {
    static constexpr bool value = true;
    using Key = K;
    using Mapped = V;
};
template <typename K, typename V> struct AssociativeTraits<QHash<K, V>> {
    static constexpr bool value = true;
    using Key = K;
    using Mapped = V;
};
template <typename K, typename V, typename C, typename A> struct AssociativeTraits<std::map<K, V, C, A>> {
    static constexpr bool value = true;
    using Key = K;
    using Mapped = V;
};
template <typename K, typename V, typename H, typename E, typename A>
struct AssociativeTraits<std::unordered_map<K, V, H, E, A>> {
    static constexpr bool value = true;
    using Key = K;
    using Mapped = V;
};

template <typename C, typename V, typename = void> struct CanPushBack : std::false_type {};
template <typename C, typename V>
struct CanPushBack<C, V, std::void_t<decltype(std::declval<C &>().push_back(std::declval<const V &>()))>>
    : std::true_type {};

template <typename C> struct SequenceInterfaceFor {
    using V = typename SequenceTraits<C>::Value;

    static const QMetaTypeInterface *valueType() { return QMetaType::fromType<V>().iface(); }
    static qsizetype size(const void *c) { return static_cast<qsizetype>(static_cast<const C *>(c)->size()); }
    static void valueAt(const void *c, qsizetype i, void *dst) {
        const C &container = *static_cast<const C *>(c);
        auto it = container.begin();
        std::advance(it, i);
        new (dst) V(*it);
    }
    static bool append(void *c, const void *v) {
        C &container = *static_cast<C *>(c);
        const V &item = *static_cast<const V *>(v);
        if constexpr (CanPushBack<C, V>::value)
            container.push_back(item);
        else
            container.insert(item);
        return true;
    }
    static void clear(void *c) { static_cast<C *>(c)->clear(); }
    static const QtMetaContainerPrivate::QMetaSequenceInterface *metaSequence();

    static inline constexpr SequenceInterface value = {&valueType, &size, &valueAt, &append, &clear, &metaSequence};
};

template <typename C, typename K, typename M, typename = void> struct HasInsertOrAssign : std::false_type {};
template <typename C, typename K, typename M>
struct HasInsertOrAssign<
    C, K, M,
    std::void_t<decltype(std::declval<C &>().insert_or_assign(std::declval<const K &>(), std::declval<const M &>()))>>
    : std::true_type {};

template <typename It, typename = void> struct HasKeyMember : std::false_type {};
template <typename It>
struct HasKeyMember<It, std::void_t<decltype(std::declval<const It &>().key())>> : std::true_type {};

template <typename It> decltype(auto) assocKey(const It &it) {
    if constexpr (HasKeyMember<It>::value)
        return it.key();
    else
        return (it->first);
}
template <typename It> decltype(auto) assocMapped(const It &it) {
    if constexpr (HasKeyMember<It>::value)
        return it.value();
    else
        return (it->second);
}

template <typename C> struct AssociativeInterfaceFor {
    using K = typename AssociativeTraits<C>::Key;
    using M = typename AssociativeTraits<C>::Mapped;

    static const QMetaTypeInterface *keyType() { return QMetaType::fromType<K>().iface(); }
    static const QMetaTypeInterface *mappedType() { return QMetaType::fromType<M>().iface(); }
    static qsizetype size(const void *c) { return static_cast<qsizetype>(static_cast<const C *>(c)->size()); }
    static void keyAt(const void *c, qsizetype i, void *dst) {
        auto it = static_cast<const C *>(c)->begin();
        std::advance(it, i);
        new (dst) K(assocKey(it));
    }
    static void mappedAt(const void *c, qsizetype i, void *dst) {
        auto it = static_cast<const C *>(c)->begin();
        std::advance(it, i);
        new (dst) M(assocMapped(it));
    }
    static qsizetype find(const void *c, const void *key) {
        const C &container = *static_cast<const C *>(c);
        const auto it = container.find(*static_cast<const K *>(key));
        if (it == container.end())
            return -1;
        return static_cast<qsizetype>(std::distance(container.begin(), it));
    }
    static bool insert(void *c, const void *key, const void *mapped) {
        C &container = *static_cast<C *>(c);
        const K &k = *static_cast<const K *>(key);
        const M &m = *static_cast<const M *>(mapped);
        if constexpr (HasInsertOrAssign<C, K, M>::value)
            container.insert_or_assign(k, m);
        else
            container.insert(k, m);
        return true;
    }
    static void clear(void *c) { static_cast<C *>(c)->clear(); }
    static const QtMetaContainerPrivate::QMetaAssociationInterface *metaAssociation();

    static inline constexpr AssociativeInterface value = {&keyType, &mappedType, &size,  &keyAt,          &mappedAt,
                                                          &find,    &insert,     &clear, &metaAssociation};
};

template <typename C, typename = void> struct SequenceCopyable : std::false_type {};
template <typename C>
struct SequenceCopyable<C, std::enable_if_t<SequenceTraits<C>::value>>
    : std::bool_constant<std::is_copy_constructible_v<typename SequenceTraits<C>::Value>> {};
template <typename C, typename = void> struct AssociativeCopyable : std::false_type {};
template <typename C>
struct AssociativeCopyable<C, std::enable_if_t<AssociativeTraits<C>::value>>
    : std::bool_constant<std::is_copy_constructible_v<typename AssociativeTraits<C>::Key> &&
                         std::is_copy_constructible_v<typename AssociativeTraits<C>::Mapped>> {};

template <typename T> constexpr const SequenceInterface *sequenceOf() {
    if constexpr (SequenceCopyable<T>::value)
        return &SequenceInterfaceFor<T>::value;
    else
        return nullptr;
}
template <typename T> constexpr const AssociativeInterface *associativeOf() {
    if constexpr (AssociativeCopyable<T>::value)
        return &AssociativeInterfaceFor<T>::value;
    else
        return nullptr;
}

} // namespace QtPrivate

// The definitions of SequenceInterfaceFor<C>::metaSequence() and AssociativeInterfaceFor<C>::metaAssociation().
#include <QtCore/qmetacontainer.h>
