// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <variant>

class QDataStream;
class QDebug;
class QObject;
class QVariant;
class QSequentialIterable;
class QAssociativeIterable;
class QLatin1StringView;
class QPartialOrdering;

using QVariantList = QList<QVariant>;
using QVariantMap = QMap<QString, QVariant>;
using QVariantHash = QHash<QString, QVariant>;
using QVariantPair = std::pair<QVariant, QVariant>;

namespace QtPrivate {

// Storage for a value of a registered type; it may be shared between variants.
struct VariantData {
    const QMetaTypeInterface *type = nullptr;
    std::shared_ptr<void> data;
    bool isNull = false;
};

void *allocateValue(const QMetaTypeInterface *type);
void freeValue(const QMetaTypeInterface *type, void *where);

template <typename T, typename... Args>
std::shared_ptr<void> makeValue(const QMetaTypeInterface *type, Args &&...args) {
    void *mem = allocateValue(type);
    try {
        new (mem) T(std::forward<Args>(args)...);
    } catch (...) {
        freeValue(type, mem);
        throw;
    }
    return std::shared_ptr<void>(mem, [type](void *p) {
        type->dtor(type, p);
        freeValue(type, p);
    });
}

template <typename T> struct IsQObjectPointer : std::false_type {};
template <typename T> struct IsQObjectPointer<T *> : std::conjunction<IsComplete<T>, std::is_base_of<QObject, T>> {};

} // namespace QtPrivate

/*!
    \class QVariant
    \brief The QVariant class acts like a union for the most common Nt data types.

    A QVariant holds a single value of one type at a time: a number, a string,
    a date, a list or map of other variants, or any type registered with
    QMetaType. It is what makes properties, \c QObject::property() and
    \c QMetaProperty work with values of any type.

    \code
    QVariant v = 42;
    v.toInt();               // 42
    v.toString();            // "42"
    v = QString("hello");
    v.typeId();              // QMetaType::QString

    struct Point3D { int x, y, z; };
    Q_DECLARE_METATYPE(Point3D)
    QVariant p = QVariant::fromValue(Point3D{1, 2, 3});
    Point3D q = p.value<Point3D>();
    \endcode

    A default-constructed QVariant is invalid. A QVariant is \e null if it
    holds no value, a null pointer, or a value of a built-in type that is
    itself null, such as an empty QString, an invalid QDate or a null QUuid.

    Conversions between types follow the table in QMetaType::convert(). The
    \c toXxx() functions convert and return a default value if the
    conversion is not possible; \c convert() changes the variant in place. A
    value that can be converted in either direction between two types
    compares equal with \c operator==().

    A QVariant can be streamed to a QDataStream in Qt's wire format and
    printed with QDebug.

    A value can also be built in place and reached without a copy:

    \code
    QVariant v(std::in_place_type<QString>, 3, QChar('x'));   // "xxx"
    if (QString *s = get_if<QString>(&v))
        s->append("!");
    v.emplace<QStringList>({"a", "b"});
    QVariant w = QVariant::fromStdVariant(std::variant<int, QString>(7));
    \endcode

    \differences
    There are no CBOR conversions, since Nt has no CBOR classes. Constructors
    for \c long and \c{unsigned long} exist because Nt's \c qint64 is
    \c long on Linux; they store a \c qlonglong or \c qulonglong. A
    \c QVariantPair is a \c std::pair, which is what QPair is. \c get() and
    \c get_if() are function templates in the global namespace, not hidden
    friends, so they are found without a \c using declaration. The
    \c QVariant::Private layout is not provided. Copies of a variant share
    the stored value until one is changed, but \c detach() and
    \c isDetached() are not part of the API.
    \enddifferences

    \sa QMetaType, QSequentialIterable, QAssociativeIterable
*/
class QVariant {
public:
    /*!
        \enum QVariant::Type

        The type a variant holds, using the ids of QMetaType::Type.

        \value Invalid The variant holds no value.
        \value Bool \c bool.
        \value Int \c int.
        \value UInt \c{unsigned int}.
        \value LongLong \c{long long}.
        \value ULongLong \c{unsigned long long}.
        \value Double \c double.
        \value Char QChar.
        \value Map QVariantMap.
        \value List QVariantList.
        \value String QString.
        \value StringList QStringList.
        \value ByteArray QByteArray.
        \value BitArray QBitArray.
        \value Date QDate.
        \value Time QTime.
        \value DateTime QDateTime.
        \value Url QUrl.
        \value Locale QLocale.
        \value Rect QRect.
        \value RectF QRectF.
        \value Size QSize.
        \value SizeF QSizeF.
        \value Line QLine.
        \value LineF QLineF.
        \value Point QPoint.
        \value PointF QPointF.
        \value RegularExpression QRegularExpression.
        \value Hash QVariantHash.
        \value EasingCurve QEasingCurve.
        \value Uuid QUuid.
        \value ModelIndex QModelIndex.
        \value PersistentModelIndex QPersistentModelIndex.
        \value UserType The first id of a registered user type.
        \value LastType The largest value of the enumeration.
    */
    enum Type {
        Invalid = QMetaType::UnknownType,
        Bool = QMetaType::Bool,
        Int = QMetaType::Int,
        UInt = QMetaType::UInt,
        LongLong = QMetaType::LongLong,
        ULongLong = QMetaType::ULongLong,
        Double = QMetaType::Double,
        Char = QMetaType::QChar,
        Map = QMetaType::QVariantMap,
        List = QMetaType::QVariantList,
        String = QMetaType::QString,
        StringList = QMetaType::QStringList,
        ByteArray = QMetaType::QByteArray,
        BitArray = QMetaType::QBitArray,
        Date = QMetaType::QDate,
        Time = QMetaType::QTime,
        DateTime = QMetaType::QDateTime,
        Url = QMetaType::QUrl,
        Locale = QMetaType::QLocale,
        Rect = QMetaType::QRect,
        RectF = QMetaType::QRectF,
        Size = QMetaType::QSize,
        SizeF = QMetaType::QSizeF,
        Line = QMetaType::QLine,
        LineF = QMetaType::QLineF,
        Point = QMetaType::QPoint,
        PointF = QMetaType::QPointF,
        RegularExpression = QMetaType::QRegularExpression,
        Hash = QMetaType::QVariantHash,
        EasingCurve = QMetaType::QEasingCurve,
        Uuid = QMetaType::QUuid,
        ModelIndex = QMetaType::QModelIndex,
        PersistentModelIndex = QMetaType::QPersistentModelIndex,
        UserType = QMetaType::User,
        LastType = 0xffffffff
    };

private:
    // Enables the in-place constructors and emplace() for a type a variant can hold.
    template <typename T, typename... Args>
    using if_in_place =
        std::enable_if_t<std::conjunction_v<std::is_same<T, std::remove_cv_t<std::remove_reference_t<T>>>,
                                            std::negation<std::is_same<T, QVariant>>, std::is_copy_constructible<T>,
                                            std::is_destructible<T>, std::is_constructible<T, Args...>>,
                         bool>;

public:
    QVariant() noexcept = default;
    ~QVariant() = default;
    explicit QVariant(QMetaType type, const void *copy = nullptr);
    QVariant(const QVariant &other) = default;
    QVariant(QVariant &&other) noexcept = default;
    QVariant(int i);
    QVariant(uint ui);
    QVariant(qlonglong ll);
    QVariant(qulonglong ull);
    QVariant(long l);
    QVariant(unsigned long ul);
    QVariant(bool b);
    QVariant(double d);
    QVariant(float f);
    QVariant(const char *str);
    QVariant(QLatin1StringView str);
    QVariant(const QByteArray &bytearray);
    QVariant(const QString &string);
    QVariant(QChar qchar);
    QVariant(const QStringList &stringlist);
    QVariant(std::nullptr_t);
    QVariant(QDate date);
    QVariant(QTime time);
    QVariant(const QDateTime &datetime);
    QVariant(const QVariantList &list);
    QVariant(const QVariantMap &map);
    QVariant(const QVariantHash &hash);
    QVariant(const QSize &size);
    QVariant(const QSizeF &size);
    QVariant(const QPoint &pt);
    QVariant(const QPointF &pt);
    QVariant(const QLine &line);
    QVariant(const QLineF &line);
    QVariant(const QRect &rect);
    QVariant(const QRectF &rect);
    QVariant(const QUrl &url);
    QVariant(const QBitArray &bitarray);
    QVariant(const QLocale &locale);
    QVariant(const QRegularExpression &re);
    QVariant(const QEasingCurve &easing);
    QVariant(const QUuid &uuid);
    QVariant(const QModelIndex &modelIndex);
    QVariant(const QPersistentModelIndex &modelIndex);
    QVariant(const QJsonValue &jsonValue);
    QVariant(const QJsonObject &jsonObject);
    QVariant(const QJsonArray &jsonArray);
    QVariant(const QJsonDocument &jsonDocument);

    /*!
        Constructs a variant that holds a \c T, which is constructed in place from \a args.
        The first argument is \c{std::in_place_type<T>} and only selects the type.

        \code
        QVariant v(std::in_place_type<QString>, 3, QChar('x'));   // holds "xxx"
        \endcode

        \since 6.6
        \sa emplace(), fromValue()
    */
    template <typename T, typename... Args, if_in_place<T, Args...> = true>
    explicit QVariant(std::in_place_type_t<T>, Args &&...args) {
        d.type = QMetaType::fromType<T>().iface();
        d.data = QtPrivate::makeValue<T>(d.type, std::forward<Args>(args)...);
    }

    /*!
        \overload
        Constructs a variant that holds a \c T, which is constructed in place from the
        initializer list \a il and \a args.

        \since 6.6
    */
    template <typename T, typename U, typename... Args, if_in_place<T, std::initializer_list<U> &, Args...> = true>
    explicit QVariant(std::in_place_type_t<T>, std::initializer_list<U> il, Args &&...args) {
        d.type = QMetaType::fromType<T>().iface();
        d.data = QtPrivate::makeValue<T>(d.type, il, std::forward<Args>(args)...);
    }

    QVariant &operator=(const QVariant &other) = default;
    QVariant &operator=(QVariant &&other) noexcept = default;

    void swap(QVariant &other) noexcept { std::swap(d, other.d); }

    Type type() const;
    int userType() const { return typeId(); }
    int typeId() const;
    QMetaType metaType() const;
    const char *typeName() const;

    bool canConvert(QMetaType targetType) const;
    bool convert(QMetaType targetType);

    bool isValid() const noexcept { return d.type != nullptr; }
    bool isNull() const;
    void clear();

    int toInt(bool *ok = nullptr) const;
    uint toUInt(bool *ok = nullptr) const;
    qlonglong toLongLong(bool *ok = nullptr) const;
    qulonglong toULongLong(bool *ok = nullptr) const;
    bool toBool() const;
    double toDouble(bool *ok = nullptr) const;
    float toFloat(bool *ok = nullptr) const;
    qreal toReal(bool *ok = nullptr) const;
    QByteArray toByteArray() const;
    QBitArray toBitArray() const;
    QString toString() const;
    QStringList toStringList() const;
    QChar toChar() const;
    QDate toDate() const;
    QTime toTime() const;
    QDateTime toDateTime() const;
    QVariantList toList() const;
    QVariantMap toMap() const;
    QVariantHash toHash() const;
    QPoint toPoint() const;
    QPointF toPointF() const;
    QRect toRect() const;
    QSize toSize() const;
    QSizeF toSizeF() const;
    QLine toLine() const;
    QLineF toLineF() const;
    QRectF toRectF() const;
    QUrl toUrl() const;
    QLocale toLocale() const;
    QRegularExpression toRegularExpression() const;
    QEasingCurve toEasingCurve() const;
    QUuid toUuid() const;
    QModelIndex toModelIndex() const;
    QPersistentModelIndex toPersistentModelIndex() const;
    QJsonValue toJsonValue() const;
    QJsonObject toJsonObject() const;
    QJsonArray toJsonArray() const;
    QJsonDocument toJsonDocument() const;

    void load(QDataStream &ds);
    void save(QDataStream &ds) const;

    /*!
        Returns a variant that holds a copy of \a value. A QVariant argument is returned
        unchanged.

        \sa setValue(), value()
    */
    template <typename T> static QVariant fromValue(T &&value) {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<U, QVariant>) {
            return std::forward<T>(value);
        } else {
            QVariant v;
            const bool isNull = isNullPointer(value);
            v.d.type = QMetaType::fromType<U>().iface();
            v.d.data = QtPrivate::makeValue<U>(v.d.type, std::forward<T>(value));
            v.d.isNull = isNull;
            return v;
        }
    }

    /*!
        Returns a variant of the type described by \a type that holds a copy of the value at
        \a copy, or a default-constructed value if \a copy is \c nullptr.

        \sa fromValue()
    */
    static QVariant fromMetaType(QMetaType type, const void *copy = nullptr) { return QVariant(type, copy); }

    /*!
        Returns a variant that holds the active alternative of \a value. The result is invalid
        if the alternative is \c std::monostate or if \a value is valueless by exception.

        \since 6.6
        \sa fromValue()
    */
    template <typename... Types> static QVariant fromStdVariant(const std::variant<Types...> &value) {
        if (value.valueless_by_exception())
            return QVariant();
        return std::visit([](const auto &alternative) { return fromAlternative(alternative); }, value);
    }

    /*!
        \overload
        Returns a variant that holds the active alternative of \a value, which is moved from.

        \since 6.6
    */
    template <typename... Types> static QVariant fromStdVariant(std::variant<Types...> &&value) {
        if (value.valueless_by_exception())
            return QVariant();
        return std::visit(
            [](auto &&alternative) { return fromAlternative(std::forward<decltype(alternative)>(alternative)); },
            std::move(value));
    }

    /*!
        Replaces what the variant holds with a \c T that is constructed in place from \a args,
        and returns a reference to the new value.

        \since 6.6
        \sa setValue()
    */
    template <typename T, typename... Args, if_in_place<T, Args...> = true> T &emplace(Args &&...args) {
        *this = QVariant(std::in_place_type<T>, std::forward<Args>(args)...);
        return *static_cast<T *>(d.data.get());
    }

    /*!
        \overload
        Replaces what the variant holds with a \c T that is constructed in place from the
        initializer list \a il and \a args, and returns a reference to the new value.

        \since 6.6
    */
    template <typename T, typename U, typename... Args, if_in_place<T, std::initializer_list<U> &, Args...> = true>
    T &emplace(std::initializer_list<U> il, Args &&...args) {
        *this = QVariant(std::in_place_type<T>, il, std::forward<Args>(args)...);
        return *static_cast<T *>(d.data.get());
    }

    /*!
        Stores a copy of \a value in the variant, replacing what it held.

        \sa fromValue(), value()
    */
    template <typename T> void setValue(T &&value) { *this = fromValue(std::forward<T>(value)); }

    /*!
        Returns the stored value converted to \c T, or a default-constructed \c T if the
        variant cannot be converted. If \c T is a pointer to a QObject-derived class and the
        variant holds a pointer to a QObject, the pointer is cast with \c dynamic_cast.

        \sa setValue(), fromValue(), canConvert(), convert()
    */
    template <typename T> T value() const;

    /*!
        Returns \c true if the variant can be converted to \c T; otherwise returns \c false.

        \sa convert(), value()
    */
    template <typename T> bool canConvert() const;

    /*!
        \since 6.0
        Returns \c true if view() can give a view of the value as a \c T, such as a QSequentialIterable of a list;
        otherwise returns \c false.

        \sa view(), QMetaType::canView()
    */
    template <typename T> bool canView() const { return QMetaType::canView(metaType(), QMetaType::fromType<T>()); }
    /*!
        \since 6.0
        Returns a view of the value as a \c T, through which the value in this variant can be changed, or a
        default-constructed \c T if there is none. The variant is detached first, and the view refers to its
        value: it must not outlive the variant or be used after the variant changes.

        \code
        QVariant v = QVariant::fromValue(QList<int>{1, 2});
        QSequentialIterable list = v.view<QSequentialIterable>();
        list.addValue(3); // v now holds {1, 2, 3}
        \endcode

        \sa canView(), QMetaType::view()
    */
    template <typename T> T view() {
        T t{};
        if (isValid())
            QMetaType::view(metaType(), data(), QMetaType::fromType<T>(), &t);
        return t;
    }

    /*!
        \overload
        Returns \c true if the variant can be converted to the type with the id \a targetTypeId.
    */
    bool canConvert(int targetTypeId) const { return canConvert(QMetaType(targetTypeId)); }
    /*!
        \overload
        Converts the variant to the type with the id \a targetTypeId and returns \c true on
        success. On failure the variant becomes invalid.
    */
    bool convert(int targetTypeId) { return convert(QMetaType(targetTypeId)); }

    const void *constData() const;
    /*!
        Returns a pointer to the stored value, or \c nullptr if the variant is invalid. If the
        value is shared with other variants, it is copied first.
    */
    void *data();
    /*!
        Returns a pointer to the stored value, or \c nullptr if the variant is invalid.
    */
    const void *data() const { return constData(); }

    /*!
        Returns \c true if \a a and \a b have the same type and value, or if the type of one can
        be converted to the other and the converted values are equal. Numbers compare by value
        whatever their type.
    */
    friend bool operator==(const QVariant &a, const QVariant &b) { return a.equals(b); }
    /*!
        Returns \c true if \a a and \a b are not equal.
    */
    friend bool operator!=(const QVariant &a, const QVariant &b) { return !a.equals(b); }

    static QPartialOrdering compare(const QVariant &lhs, const QVariant &rhs);

    static const char *typeToName(int typeId);
    static Type nameToType(const char *name);

private:
    template <typename T> static QVariant fromAlternative(T &&alternative) {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::monostate>)
            return QVariant();
        else
            return fromValue(std::forward<T>(alternative));
    }

    template <typename T> static bool isNullPointer(const T &value) {
        if constexpr (std::is_pointer_v<T> || std::is_same_v<T, std::nullptr_t>)
            return value == nullptr;
        else
            return false;
    }

    bool equals(const QVariant &other) const;
    bool convertInto(QMetaType target, void *result) const;
    const QtPrivate::SequenceInterface *sequenceInterface() const;
    const QtPrivate::AssociativeInterface *associativeInterface() const;

    friend class QSequentialIterable;
    friend class QAssociativeIterable;

    QtPrivate::VariantData d;
};

QDataStream &operator<<(QDataStream &s, const QVariant &v);
QDataStream &operator>>(QDataStream &s, QVariant &v);
QDebug operator<<(QDebug dbg, const QVariant &v);

#include <QtCore/qsequentialiterable.h>
#include <QtCore/qassociativeiterable.h>

template <typename T> T QVariant::value() const {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, QVariant>) {
        return *this;
    } else if constexpr (std::is_same_v<U, QSequentialIterable> || std::is_same_v<U, QAssociativeIterable>) {
        return U(*this);
    } else {
        const QMetaType target = QMetaType::fromType<U>();
        if (metaType() == target)
            return *static_cast<const U *>(constData());
        if constexpr (QtPrivate::IsQObjectPointer<U>::value) {
            if (metaType().flags().testFlag(QMetaType::PointerToQObject) || typeId() == QMetaType::QObjectStar)
                return dynamic_cast<U>(*static_cast<QObject *const *>(constData()));
        }
        U result{};
        if (isValid() && convertInto(target, static_cast<void *>(&result)))
            return result;
        return U();
    }
}

template <typename T> bool QVariant::canConvert() const {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, QSequentialIterable>) {
        return sequenceInterface() != nullptr;
    } else if constexpr (std::is_same_v<U, QAssociativeIterable>) {
        return associativeInterface() != nullptr;
    } else {
        return canConvert(QMetaType::fromType<U>());
    }
}

// Returns the value of `value` converted to T, as QVariant::value() does.
template <typename T> inline T qvariant_cast(const QVariant &value) { return value.template value<T>(); }

// Returns a pointer to the value in *v if it holds exactly a T, and nullptr otherwise (also when v
// is nullptr). The value is not converted. The non-const overload detaches the value from copies
// of the variant first.
template <typename T> T *get_if(QVariant *v) noexcept {
    if (!v || !v->isValid() || v->metaType() != QMetaType::fromType<T>())
        return nullptr;
    return static_cast<T *>(v->data());
}
template <typename T> const T *get_if(const QVariant *v) noexcept {
    if (!v || !v->isValid() || v->metaType() != QMetaType::fromType<T>())
        return nullptr;
    return static_cast<const T *>(v->constData());
}

// Returns the value in v, which must hold exactly a T: the behaviour is undefined otherwise.
template <typename T> T &get(QVariant &v) { return *get_if<T>(&v); }
template <typename T> const T &get(const QVariant &v) { return *get_if<T>(&v); }
template <typename T> T &&get(QVariant &&v) { return std::move(*get_if<T>(&v)); }
template <typename T> const T &&get(const QVariant &&v) { return std::move(*get_if<T>(&v)); }
