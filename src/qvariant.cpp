// MIT License - see LICENSE
#include <QtCore/qvariant.h>

#include <QtCore/qbitarray.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qline.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>

#include "qmetatype_p.h"

#include <cmath>

namespace {

bool isNumeric(int id) {
    switch (id) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Long:
    case QMetaType::Short:
    case QMetaType::Char:
    case QMetaType::ULong:
    case QMetaType::UShort:
    case QMetaType::UChar:
    case QMetaType::Float:
    case QMetaType::SChar:
    case QMetaType::Char16:
    case QMetaType::Char32:
        return true;
    default:
        return false;
    }
}

bool isFloating(int id) { return id == QMetaType::Double || id == QMetaType::Float; }

bool isUnsigned(int id) {
    switch (id) {
    case QMetaType::Bool:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::ULong:
    case QMetaType::UShort:
    case QMetaType::UChar:
    case QMetaType::Char16:
    case QMetaType::Char32:
        return true;
    default:
        return false;
    }
}

// A numeric variant as the widest type that holds it.
struct Wide {
    bool real = false;
    bool isUnsigned = false;
    qlonglong s = 0;
    qulonglong u = 0;
    double d = 0;
};

bool wideOf(const QVariant &v, Wide &w) {
    const int id = v.typeId();
    const void *p = v.constData();
    if (isFloating(id)) {
        w.real = true;
        w.d = id == QMetaType::Float ? static_cast<double>(*static_cast<const float *>(p))
                                     : *static_cast<const double *>(p);
        return true;
    }
    if (!isNumeric(id))
        return false;
    if (isUnsigned(id)) {
        w.isUnsigned = true;
        qulonglong u = 0;
        switch (v.metaType().sizeOf()) {
        case 1:
            u = *static_cast<const unsigned char *>(p);
            break;
        case 2:
            u = *static_cast<const unsigned short *>(p);
            break;
        case 4:
            u = *static_cast<const unsigned *>(p);
            break;
        default:
            u = *static_cast<const unsigned long long *>(p);
            break;
        }
        w.u = u;
        return true;
    }
    qlonglong s = 0;
    switch (v.metaType().sizeOf()) {
    case 1:
        s = *static_cast<const signed char *>(p); // NOLINT(bugprone-signed-char-misuse): a number, not a character
        break;
    case 2:
        s = *static_cast<const short *>(p);
        break;
    case 4:
        s = *static_cast<const int *>(p);
        break;
    default:
        s = *static_cast<const long long *>(p);
        break;
    }
    w.s = s;
    return true;
}

bool equalNumbers(const Wide &a, const Wide &b) {
    if (a.real || b.real) {
        const auto asDouble = [](const Wide &w) {
            return w.real ? w.d : (w.isUnsigned ? static_cast<double>(w.u) : static_cast<double>(w.s));
        };
        return asDouble(a) == asDouble(b);
    }
    if (a.isUnsigned == b.isUnsigned)
        return a.isUnsigned ? a.u == b.u : a.s == b.s;
    const Wide &sgn = a.isUnsigned ? b : a;
    const Wide &uns = a.isUnsigned ? a : b;
    return sgn.s >= 0 && static_cast<qulonglong>(sgn.s) == uns.u;
}

template <typename T> T numberFrom(const QVariant &v, bool *ok) {
    const QMetaType target = QMetaType::fromType<T>();
    T result{};
    bool good = false;
    if (v.isValid()) {
        if (v.metaType() == target) {
            result = *static_cast<const T *>(v.constData());
            good = true;
        } else {
            good = QMetaType::convert(v.metaType(), v.constData(), target, &result);
            if (!good)
                result = T();
        }
    }
    if (ok)
        *ok = good;
    return result;
}

} // namespace

QVariant::QVariant(QMetaType type, const void *copy) {
    if (!type.isValid())
        return;
    // A QVariant is never nested in another.
    if (type.iface() == QMetaType::fromType<QVariant>().iface()) {
        if (copy)
            *this = *static_cast<const QVariant *>(copy);
        return;
    }
    void *p = type.create(copy);
    if (!p)
        return;
    d.type = type.iface();
    d.data = std::shared_ptr<void>(p, [type](void *ptr) { type.destroy(ptr); });
    d.isNull = copy == nullptr;
    if (copy && (type.iface()->flags & QtPrivate::kFlagIsPointer))
        d.isNull = *static_cast<void *const *>(copy) == nullptr;
}

QVariant::QVariant(int i) { *this = fromValue(i); }
QVariant::QVariant(uint ui) { *this = fromValue(ui); }
QVariant::QVariant(qlonglong ll) { *this = fromValue(ll); }
QVariant::QVariant(qulonglong ull) { *this = fromValue(ull); }
QVariant::QVariant(long l) { *this = fromValue(static_cast<qlonglong>(l)); }
QVariant::QVariant(unsigned long ul) { *this = fromValue(static_cast<qulonglong>(ul)); }
QVariant::QVariant(bool b) { *this = fromValue(b); }
QVariant::QVariant(double dbl) { *this = fromValue(dbl); }
QVariant::QVariant(float f) { *this = fromValue(f); }
QVariant::QVariant(const char *str) { *this = fromValue(str ? QString::fromUtf8(str) : QString()); }
QVariant::QVariant(QLatin1StringView str) { *this = fromValue(QString(str)); }
QVariant::QVariant(const QByteArray &bytearray) { *this = fromValue(bytearray); }
QVariant::QVariant(const QString &string) { *this = fromValue(string); }
QVariant::QVariant(QChar qchar) { *this = fromValue(qchar); }
QVariant::QVariant(const QStringList &stringlist) { *this = fromValue(stringlist); }
QVariant::QVariant(std::nullptr_t) { *this = fromValue(nullptr); }
QVariant::QVariant(QDate date) { *this = fromValue(date); }
QVariant::QVariant(QTime time) { *this = fromValue(time); }
QVariant::QVariant(const QDateTime &datetime) { *this = fromValue(datetime); }
QVariant::QVariant(const QVariantList &list) { *this = fromValue(list); }
QVariant::QVariant(const QVariantMap &map) { *this = fromValue(map); }
QVariant::QVariant(const QVariantHash &hash) { *this = fromValue(hash); }
QVariant::QVariant(const QSize &size) { *this = fromValue(size); }
QVariant::QVariant(const QSizeF &size) { *this = fromValue(size); }
QVariant::QVariant(const QPoint &pt) { *this = fromValue(pt); }
QVariant::QVariant(const QPointF &pt) { *this = fromValue(pt); }
QVariant::QVariant(const QLine &line) { *this = fromValue(line); }
QVariant::QVariant(const QLineF &line) { *this = fromValue(line); }
QVariant::QVariant(const QRect &rect) { *this = fromValue(rect); }
QVariant::QVariant(const QRectF &rect) { *this = fromValue(rect); }
QVariant::QVariant(const QUrl &url) { *this = fromValue(url); }
QVariant::QVariant(const QBitArray &bitarray) { *this = fromValue(bitarray); }

/*!
    \fn QVariant::QVariant()
    Constructs an invalid variant.

    \sa isValid()
*/

/*!
    \fn QVariant::~QVariant()
    Destroys the variant and the value it holds, unless another variant shares it.
*/

/*!
    \fn QVariant::QVariant(const QVariant &other)
    Constructs a copy of \a other.
*/

/*!
    \fn QVariant::QVariant(QVariant &&other)
    Constructs a variant by moving \a other into it; \a other is left invalid.
*/

/*!
    \fn QVariant &QVariant::operator=(const QVariant &other)
    Assigns \a other to this variant and returns a reference to it.
*/

/*!
    \fn QVariant &QVariant::operator=(QVariant &&other)
    Moves \a other into this variant and returns a reference to it.
*/

/*!
    \fn QVariant::QVariant(QMetaType type, const void *copy)
    Constructs a variant of the type \a type. If \a copy is not \c nullptr, the variant holds a
    copy of the value at that address; otherwise it holds a default-constructed value and is
    null. The variant is invalid if \a type is invalid or its values cannot be constructed
    that way. For \c QMetaType::QVariant the result is a copy of the QVariant at \a copy, or an
    invalid variant.

    \sa fromMetaType()
*/

/*!
    \fn QVariant::QVariant(int i)
    Constructs a variant that holds the \c int \a i.
*/

/*!
    \fn QVariant::QVariant(uint ui)
    Constructs a variant that holds the unsigned integer \a ui.
*/

/*!
    \fn QVariant::QVariant(qlonglong ll)
    Constructs a variant that holds the \c qlonglong \a ll.
*/

/*!
    \fn QVariant::QVariant(qulonglong ull)
    Constructs a variant that holds the \c qulonglong \a ull.
*/

/*!
    \fn QVariant::QVariant(long l)
    Constructs a variant that holds \a l as a \c qlonglong.
*/

/*!
    \fn QVariant::QVariant(unsigned long ul)
    Constructs a variant that holds \a ul as a \c qulonglong.
*/

/*!
    \fn QVariant::QVariant(bool b)
    Constructs a variant that holds the \c bool \a b.
*/

/*!
    \fn QVariant::QVariant(double d)
    Constructs a variant that holds the \c double \a d.
*/

/*!
    \fn QVariant::QVariant(float f)
    Constructs a variant that holds the \c float \a f.
*/

/*!
    \fn QVariant::QVariant(const char *str)
    Constructs a variant that holds a QString made from the UTF-8 text \a str. A \c nullptr
    gives a null QString.
*/

/*!
    \fn QVariant::QVariant(QLatin1StringView str)
    Constructs a variant that holds a QString made from \a str.
*/

/*!
    \fn QVariant::QVariant(const QByteArray &bytearray)
    Constructs a variant that holds a copy of \a bytearray.
*/

/*!
    \fn QVariant::QVariant(const QString &string)
    Constructs a variant that holds a copy of \a string.
*/

/*!
    \fn QVariant::QVariant(QChar qchar)
    Constructs a variant that holds the character \a qchar.
*/

/*!
    \fn QVariant::QVariant(const QStringList &stringlist)
    Constructs a variant that holds a copy of \a stringlist.
*/

/*!
    \fn QVariant::QVariant(std::nullptr_t)
    Constructs a variant that holds a null pointer of type \c std::nullptr_t.
*/

/*!
    \fn QVariant::QVariant(QDate date)
    Constructs a variant that holds \a date.
*/

/*!
    \fn QVariant::QVariant(QTime time)
    Constructs a variant that holds \a time.
*/

/*!
    \fn QVariant::QVariant(const QDateTime &datetime)
    Constructs a variant that holds \a datetime.
*/

/*!
    \fn QVariant::QVariant(const QVariantList &list)
    Constructs a variant that holds a copy of \a list.
*/

/*!
    \fn QVariant::QVariant(const QVariantMap &map)
    Constructs a variant that holds a copy of \a map.
*/

/*!
    \fn QVariant::QVariant(const QVariantHash &hash)
    Constructs a variant that holds a copy of \a hash.
*/

/*!
    \fn QVariant::QVariant(const QSize &size)
    Constructs a variant that holds \a size.
*/

/*!
    \fn QVariant::QVariant(const QSizeF &size)
    Constructs a variant that holds \a size.
*/

/*!
    \fn QVariant::QVariant(const QPoint &pt)
    Constructs a variant that holds \a pt.
*/

/*!
    \fn QVariant::QVariant(const QPointF &pt)
    Constructs a variant that holds \a pt.
*/

/*!
    \fn QVariant::QVariant(const QLine &line)
    Constructs a variant that holds \a line.
*/

/*!
    \fn QVariant::QVariant(const QLineF &line)
    Constructs a variant that holds \a line.
*/

/*!
    \fn QVariant::QVariant(const QRect &rect)
    Constructs a variant that holds \a rect.
*/

/*!
    \fn QVariant::QVariant(const QRectF &rect)
    Constructs a variant that holds \a rect.
*/

/*!
    \fn QVariant::QVariant(const QUrl &url)
    Constructs a variant that holds \a url.
*/

/*!
    \fn QVariant::QVariant(const QBitArray &bitarray)
    Constructs a variant that holds \a bitarray.
*/

/*!
    \fn void QVariant::swap(QVariant &other)
    Swaps this variant with \a other.
*/

/*!
    Returns the type of the stored value as a QVariant::Type. For a type that is not a member of
    the enumeration it returns the \c QMetaType id, or \c UserType for a type registered at run
    time. Prefer typeId().

    \sa typeId(), metaType()
*/
QVariant::Type QVariant::type() const {
    const int id = typeId();
    return id >= QMetaType::User ? UserType : static_cast<Type>(id);
}

/*!
    \fn int QVariant::userType() const
    Returns the \c QMetaType id of the stored value; the same as typeId().
*/

/*!
    Returns the \c QMetaType id of the stored value, or \c QMetaType::UnknownType (0) if the
    variant is invalid.

    \sa metaType(), typeName()
*/
int QVariant::typeId() const { return d.type ? QMetaType(d.type).id() : 0; }

/*!
    Returns the QMetaType of the stored value, or an invalid QMetaType if the variant is
    invalid.

    \sa typeId()
*/
QMetaType QVariant::metaType() const { return QMetaType(d.type); }

/*!
    Returns the name of the type of the stored value, or \c nullptr if the variant is invalid.

    \sa typeId(), typeToName()
*/
const char *QVariant::typeName() const { return d.type ? QMetaType(d.type).name() : nullptr; }

/*!
    Returns \c true if the variant can be converted to \a targetType; otherwise returns
    \c false. A \c true result does not promise that every value converts: a QString that does
    not hold a number is convertible to \c int, but not to a number.

    \sa convert(), QMetaType::convert()
*/
bool QVariant::canConvert(QMetaType targetType) const {
    return d.type && QMetaType::canConvert(metaType(), targetType);
}

/*!
    Converts the variant to \a targetType and returns \c true on success. If the conversion is
    not possible, the variant becomes a null value of \a targetType and the function returns
    \c false. Converting an invalid variant does the same.

    \sa canConvert(), clear()
*/
bool QVariant::convert(QMetaType targetType) {
    if (!targetType.isValid()) {
        clear();
        return false;
    }
    if (isValid() && metaType() == targetType)
        return true;
    QVariant out(targetType);
    if (!isValid() || !out.isValid()) {
        *this = out;
        return false;
    }
    if (!QMetaType::convert(metaType(), constData(), targetType, const_cast<void *>(out.constData()))) {
        *this = out;
        return false;
    }
    out.d.isNull = false;
    *this = out;
    return true;
}

/*!
    \fn bool QVariant::isValid() const
    Returns \c true if the variant holds a value, even a null one; otherwise returns \c false.

    \sa isNull()
*/

/*!
    Returns \c true if the variant is invalid, holds a null pointer, or holds a value that is
    itself null: an empty-and-null QString, QByteArray or QBitArray, a null QChar, an invalid
    QDate, QTime or QDateTime, a null QUuid, QRect, QSize, QPoint or QLine, or a variant created
    with QVariant(QMetaType) and no value. Other types are never null.

    \sa isValid(), clear()
*/
bool QVariant::isNull() const {
    if (!d.type)
        return true;
    if (d.isNull)
        return true;
    const void *p = d.data.get();
    switch (typeId()) {
    case QMetaType::QString:
        return static_cast<const QString *>(p)->isNull();
    case QMetaType::QByteArray:
        return static_cast<const QByteArray *>(p)->isNull();
    case QMetaType::QBitArray:
        return static_cast<const QBitArray *>(p)->isNull();
    case QMetaType::QChar:
        return static_cast<const QChar *>(p)->isNull();
    case QMetaType::QDate:
        return static_cast<const QDate *>(p)->isNull();
    case QMetaType::QTime:
        return static_cast<const QTime *>(p)->isNull();
    case QMetaType::QDateTime:
        return static_cast<const QDateTime *>(p)->isNull();
    case QMetaType::QUuid:
        return static_cast<const QUuid *>(p)->isNull();
    case QMetaType::QRect:
        return static_cast<const QRect *>(p)->isNull();
    case QMetaType::QRectF:
        return static_cast<const QRectF *>(p)->isNull();
    case QMetaType::QSize:
        return static_cast<const QSize *>(p)->isNull();
    case QMetaType::QSizeF:
        return static_cast<const QSizeF *>(p)->isNull();
    case QMetaType::QPoint:
        return static_cast<const QPoint *>(p)->isNull();
    case QMetaType::QPointF:
        return static_cast<const QPointF *>(p)->isNull();
    case QMetaType::QLine:
        return static_cast<const QLine *>(p)->isNull();
    case QMetaType::QLineF:
        return static_cast<const QLineF *>(p)->isNull();
    case QMetaType::Nullptr:
        return true;
    default:
        break;
    }
    return false;
}

/*!
    Releases the value and makes the variant invalid.

    \sa isValid()
*/
void QVariant::clear() { d = QtPrivate::VariantData(); }

/*!
    Returns the variant as an \c int, converting it if needed. If \a ok is not \c nullptr, it is
    set to \c true if the conversion succeeded. A variant that cannot be converted gives 0.

    \sa canConvert(), convert()
*/
int QVariant::toInt(bool *ok) const { return numberFrom<int>(*this, ok); }

/*!
    Returns the variant as an unsigned integer, converting it if needed. If \a ok is not
    \c nullptr, it is set to \c true if the conversion succeeded.
*/
uint QVariant::toUInt(bool *ok) const { return numberFrom<uint>(*this, ok); }

/*!
    Returns the variant as a \c qlonglong, converting it if needed. If \a ok is not \c nullptr, it
    is set to \c true if the conversion succeeded.
*/
qlonglong QVariant::toLongLong(bool *ok) const { return numberFrom<qlonglong>(*this, ok); }

/*!
    Returns the variant as a \c qulonglong, converting it if needed. If \a ok is not \c nullptr,
    it is set to \c true if the conversion succeeded.
*/
qulonglong QVariant::toULongLong(bool *ok) const { return numberFrom<qulonglong>(*this, ok); }

/*!
    Returns the variant as a \c bool, converting it if needed. Zero, an empty string,
    \c{"0"} and \c{"false"} (in any case) are \c false; other numbers and strings are \c true. A
    variant that cannot be converted gives \c false.
*/
bool QVariant::toBool() const { return numberFrom<bool>(*this, nullptr); }

/*!
    Returns the variant as a \c double, converting it if needed. If \a ok is not \c nullptr, it is
    set to \c true if the conversion succeeded.
*/
double QVariant::toDouble(bool *ok) const { return numberFrom<double>(*this, ok); }

/*!
    Returns the variant as a \c float, converting it if needed. If \a ok is not \c nullptr, it is
    set to \c true if the conversion succeeded.
*/
float QVariant::toFloat(bool *ok) const { return numberFrom<float>(*this, ok); }

/*!
    Returns the variant as a \c qreal, which is a \c double. If \a ok is not \c nullptr, it is set
    to \c true if the conversion succeeded.

    \sa toDouble()
*/
qreal QVariant::toReal(bool *ok) const { return numberFrom<double>(*this, ok); }

/*!
    Returns the variant as a QByteArray, converting it if needed. A QString converts to UTF-8.
    A variant that cannot be converted gives a null QByteArray.
*/
QByteArray QVariant::toByteArray() const { return value<QByteArray>(); }

/*!
    Returns the variant as a QBitArray if it holds one; otherwise returns a null QBitArray.
*/
QBitArray QVariant::toBitArray() const { return value<QBitArray>(); }

/*!
    Returns the variant as a QString, converting it if needed: numbers and \c bool give their
    text, dates and times give ISO 8601 text, a QByteArray is read as UTF-8, and an enumeration
    declared with \c Q_ENUM() gives its key. A variant that cannot be converted gives a null
    QString.

    \sa canConvert(), QMetaType::convert()
*/
QString QVariant::toString() const { return value<QString>(); }

/*!
    Returns the variant as a QStringList, converting it if needed. A QString gives a list of
    one string, and a list of variants gives the text of each element. A variant that cannot be
    converted gives an empty list.
*/
QStringList QVariant::toStringList() const { return value<QStringList>(); }

/*!
    Returns the variant as a QChar if it holds a character, a number that is a valid UTF-16
    code unit, or a string of one character; otherwise returns a null QChar.
*/
QChar QVariant::toChar() const { return value<QChar>(); }

/*!
    Returns the variant as a QDate, converting it if needed: a QDateTime gives its date and a
    QString is read as ISO 8601. A variant that cannot be converted gives an invalid QDate.
*/
QDate QVariant::toDate() const { return value<QDate>(); }

/*!
    Returns the variant as a QTime, converting it if needed; see toDate().
*/
QTime QVariant::toTime() const { return value<QTime>(); }

/*!
    Returns the variant as a QDateTime, converting it if needed; see toDate(). A QDate gives
    the start of the day in local time.
*/
QDateTime QVariant::toDateTime() const { return value<QDateTime>(); }

/*!
    Returns the variant as a QVariantList, converting it if needed. A QStringList, a
    QByteArrayList and any registered sequential container whose elements are registered
    types convert element by element. A variant that cannot be converted gives an empty list.
*/
QVariantList QVariant::toList() const { return value<QVariantList>(); }

/*!
    Returns the variant as a QVariantMap, converting it if needed. A QVariantHash and any
    registered associative container whose keys convert to QString convert entry by entry. A
    variant that cannot be converted gives an empty map.
*/
QVariantMap QVariant::toMap() const { return value<QVariantMap>(); }

/*!
    Returns the variant as a QVariantHash, converting it if needed; see toMap().
*/
QVariantHash QVariant::toHash() const { return value<QVariantHash>(); }

/*!
    Returns the variant as a QPoint, converting it from a QPointF if needed. Otherwise returns
    a null QPoint.
*/
QPoint QVariant::toPoint() const { return value<QPoint>(); }

/*!
    Returns the variant as a QPointF, converting it from a QPoint if needed. Otherwise returns
    a null QPointF.
*/
QPointF QVariant::toPointF() const { return value<QPointF>(); }

/*!
    Returns the variant as a QRect, converting it from a QRectF if needed. Otherwise returns
    a null QRect.
*/
QRect QVariant::toRect() const { return value<QRect>(); }

/*!
    Returns the variant as a QSize, converting it from a QSizeF if needed. Otherwise returns an
    invalid QSize.
*/
QSize QVariant::toSize() const { return value<QSize>(); }

/*!
    Returns the variant as a QSizeF, converting it from a QSize if needed. Otherwise returns an
    invalid QSizeF.
*/
QSizeF QVariant::toSizeF() const { return value<QSizeF>(); }

/*!
    Returns the variant as a QLine, converting it from a QLineF if needed. Otherwise returns a
    null QLine.
*/
QLine QVariant::toLine() const { return value<QLine>(); }

/*!
    Returns the variant as a QLineF, converting it from a QLine if needed. Otherwise returns a
    null QLineF.
*/
QLineF QVariant::toLineF() const { return value<QLineF>(); }

/*!
    Returns the variant as a QRectF, converting it from a QRect if needed. Otherwise returns a
    null QRectF.
*/
QRectF QVariant::toRectF() const { return value<QRectF>(); }

/*!
    Returns the variant as a QUrl, converting it from a QString if needed. Otherwise returns an
    empty QUrl.
*/
QUrl QVariant::toUrl() const { return value<QUrl>(); }

/*!
    Returns a pointer to the stored value, or \c nullptr if the variant is invalid.

    \sa data()
*/
const void *QVariant::constData() const { return d.data.get(); }

void *QVariant::data() {
    if (!d.type)
        return nullptr;
    if (d.data.use_count() > 1) {
        const QMetaType type(d.type);
        void *copy = type.create(d.data.get());
        if (!copy)
            return d.data.get();
        d.data = std::shared_ptr<void>(copy, [type](void *ptr) { type.destroy(ptr); });
    }
    d.isNull = false;
    return d.data.get();
}

bool QVariant::convertInto(QMetaType target, void *result) const {
    return QMetaType::convert(metaType(), constData(), target, result);
}

bool QVariant::equals(const QVariant &other) const {
    if (!isValid() || !other.isValid())
        return isValid() == other.isValid();
    if (metaType() == other.metaType()) {
        if (d.data == other.d.data)
            return true;
        return metaType().equals(constData(), other.constData());
    }
    Wide a;
    Wide b;
    if (wideOf(*this, a) && wideOf(other, b))
        return equalNumbers(a, b);
    // Compare after converting the other value to this type, or this one to the other type.
    QVariant converted(other);
    if (other.canConvert(metaType()) && converted.convert(metaType()))
        return metaType().equals(constData(), converted.constData());
    converted = *this;
    if (canConvert(other.metaType()) && converted.convert(other.metaType()))
        return other.metaType().equals(other.constData(), converted.constData());
    return false;
}

const QtPrivate::SequenceInterface *QVariant::sequenceInterface() const { return d.type ? d.type->sequence : nullptr; }

const QtPrivate::AssociativeInterface *QVariant::associativeInterface() const {
    return d.type ? d.type->associative : nullptr;
}

/*!
    Returns the name of the type with the id \a typeId, or \c nullptr if there is none.

    \sa nameToType()
*/
const char *QVariant::typeToName(int typeId) {
    return typeId == QMetaType::UnknownType ? nullptr : QMetaType::typeName(typeId);
}

/*!
    Returns the type that has the name \a name, or \c Invalid if there is none. A type that is
    registered at run time gives \c UserType.

    \sa typeToName()
*/
QVariant::Type QVariant::nameToType(const char *name) {
    if (!name || !*name || std::strcmp(name, "Invalid") == 0)
        return Invalid;
    const int id = QMetaType::type(name);
    return id >= QMetaType::User ? UserType : static_cast<Type>(id);
}

/*!
    Reads the variant from the stream \a ds in the format of QVariant::save(). If the type has
    no stream operators, the stream's status is set to \c QDataStream::ReadCorruptData and the
    variant becomes invalid.

    \sa save()
*/
void QVariant::load(QDataStream &s) {
    clear();
    quint32 typeId = 0;
    s >> typeId;
    qint8 isNull = 0;
    s >> isNull;
    if (s.status() != QDataStream::Ok)
        return;
    QMetaType type;
    if (typeId == QMetaType::User) {
        char *name = nullptr;
        s >> name;
        if (s.status() != QDataStream::Ok) {
            delete[] name;
            return;
        }
        type = QMetaType::fromName(QByteArrayView(name ? name : ""));
        delete[] name;
        if (!type.isValid()) {
            s.setStatus(QDataStream::ReadCorruptData);
            return;
        }
    } else if (typeId == QMetaType::UnknownType) {
        return;
    } else {
        type = QMetaType(static_cast<int>(typeId));
        if (!type.isValid()) {
            s.setStatus(QDataStream::ReadCorruptData);
            return;
        }
    }
    QVariant v(type);
    if (!v.isValid() || !type.load(s, v.data())) {
        s.setStatus(QDataStream::ReadCorruptData);
        return;
    }
    v.d.isNull = isNull != 0;
    *this = v;
}

/*!
    Writes the variant to the stream \a ds: the type id (or \c QMetaType::User followed by the
    type name for a registered type), a flag that tells if the value is null, and the value
    itself. If the type has no stream operators, the stream's status is set to
    \c QDataStream::WriteFailed.

    \sa load()
*/
void QVariant::save(QDataStream &s) const {
    if (!isValid()) {
        s << static_cast<quint32>(QMetaType::UnknownType) << static_cast<qint8>(1);
        return;
    }
    const QMetaType type = metaType();
    const int id = type.id();
    s << static_cast<quint32>(id >= QMetaType::User ? static_cast<int>(QMetaType::User) : id);
    s << static_cast<qint8>(isNull() ? 1 : 0);
    if (id >= QMetaType::User)
        s << type.name();
    if (!type.save(s, constData()))
        s.setStatus(QDataStream::WriteFailed);
}

// Writes the variant v to the stream s and returns a reference to the stream.
QDataStream &operator<<(QDataStream &s, const QVariant &v) {
    v.save(s);
    return s;
}

// Reads a variant from the stream s into v and returns a reference to the stream.
QDataStream &operator>>(QDataStream &s, QVariant &v) {
    v.load(s);
    return s;
}

// Writes v to the debug stream dbg, as QVariant(int, 42). An invalid variant is written as
// QVariant(Invalid), and a value whose type cannot be printed as the type name and nothing else.
QDebug operator<<(QDebug dbg, const QVariant &v) {
    const bool spaces = dbg.autoInsertSpaces();
    dbg.nospace() << "QVariant(";
    if (!v.isValid()) {
        dbg << "Invalid";
    } else {
        dbg << v.typeName() << ", ";
        v.metaType().debugStream(dbg, v.constData());
    }
    dbg << ")";
    dbg.setAutoInsertSpaces(spaces);
    return dbg.maybeSpace();
}
