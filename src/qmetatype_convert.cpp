// MIT License - see LICENSE
// Built-in conversions between meta-types; see QMetaType::convert().
#include <QtCore/qbitarray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qline.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qvariant.h>
#include <QtCore/qversionnumber.h>

#include "qmetatype_p.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace QtPrivate {

QString numberToString(double value) {
    if (std::isnan(value))
        return QStringLiteral("nan");
    if (std::isinf(value))
        return QString::fromLatin1(value < 0 ? "-inf" : "inf");
    char buf[40];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof buf, "%.*g", precision, value);
        if (std::strtod(buf, nullptr) == value)
            break;
    }
    return QString::fromLatin1(buf);
}

QString numberToString(float value) {
    if (std::isnan(value))
        return QStringLiteral("nan");
    if (std::isinf(value))
        return QString::fromLatin1(value < 0 ? "-inf" : "inf");
    char buf[40];
    for (int precision = 1; precision <= 9; ++precision) {
        std::snprintf(buf, sizeof buf, "%.*g", precision, static_cast<double>(value));
        if (std::strtof(buf, nullptr) == value)
            break;
    }
    return QString::fromLatin1(buf);
}

namespace {

struct Number {
    enum Kind { Signed, Unsigned, Real } kind = Signed;
    qlonglong s = 0;
    qulonglong u = 0;
    double d = 0;
};

bool isNumericId(int id) {
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

bool isEnumType(QMetaType t) { return t.flags().testFlag(QMetaType::IsEnumeration); }

template <typename T> Number signedNumber(const void *p) {
    Number n;
    n.kind = Number::Signed;
    n.s = static_cast<qlonglong>(
        *static_cast<const T *>(p)); // NOLINT(bugprone-signed-char-misuse): a number, not a character
    return n;
}
template <typename T> Number unsignedNumber(const void *p) {
    Number n;
    n.kind = Number::Unsigned;
    n.u = static_cast<qulonglong>(*static_cast<const T *>(p));
    return n;
}
template <typename T> Number realNumber(const void *p) {
    Number n;
    n.kind = Number::Real;
    n.d = static_cast<double>(*static_cast<const T *>(p));
    return n;
}

bool readNumber(int id, const void *p, Number &out) {
    switch (id) {
    case QMetaType::Bool:
        out = unsignedNumber<bool>(p);
        return true;
    case QMetaType::Int:
        out = signedNumber<int>(p);
        return true;
    case QMetaType::UInt:
        out = unsignedNumber<unsigned>(p);
        return true;
    case QMetaType::LongLong:
        out = signedNumber<long long>(p);
        return true;
    case QMetaType::ULongLong:
        out = unsignedNumber<unsigned long long>(p);
        return true;
    case QMetaType::Double:
        out = realNumber<double>(p);
        return true;
    case QMetaType::Long:
        out = signedNumber<long>(p);
        return true;
    case QMetaType::Short:
        out = signedNumber<short>(p);
        return true;
    case QMetaType::Char:
        out = signedNumber<char>(p);
        return true;
    case QMetaType::ULong:
        out = unsignedNumber<unsigned long>(p);
        return true;
    case QMetaType::UShort:
        out = unsignedNumber<unsigned short>(p);
        return true;
    case QMetaType::UChar:
        out = unsignedNumber<unsigned char>(p);
        return true;
    case QMetaType::Float:
        out = realNumber<float>(p);
        return true;
    case QMetaType::SChar:
        out = signedNumber<signed char>(p);
        return true;
    case QMetaType::Char16:
        out = unsignedNumber<char16_t>(p);
        return true;
    case QMetaType::Char32:
        out = unsignedNumber<char32_t>(p);
        return true;
    default:
        return false;
    }
}

template <typename T> bool storeIntegral(void *p, const Number &n) {
    switch (n.kind) {
    case Number::Signed:
        *static_cast<T *>(p) = static_cast<T>(n.s);
        return true;
    case Number::Unsigned:
        *static_cast<T *>(p) = static_cast<T>(n.u);
        return true;
    case Number::Real: {
        if (!std::isfinite(n.d))
            return false;
        const double r = std::round(n.d);
        const double lo = static_cast<double>(std::numeric_limits<T>::min());
        const double hi = static_cast<double>(std::numeric_limits<T>::max()) + 1.0;
        if (!(r >= lo && r < hi))
            return false;
        *static_cast<T *>(p) = static_cast<T>(r);
        return true;
    }
    }
    return false;
}

template <typename T> bool storeReal(void *p, const Number &n) {
    switch (n.kind) {
    case Number::Signed:
        *static_cast<T *>(p) = static_cast<T>(n.s);
        return true;
    case Number::Unsigned:
        *static_cast<T *>(p) = static_cast<T>(n.u);
        return true;
    case Number::Real:
        *static_cast<T *>(p) = static_cast<T>(n.d);
        return true;
    }
    return false;
}

bool storeBool(void *p, const Number &n) {
    switch (n.kind) {
    case Number::Signed:
        *static_cast<bool *>(p) = n.s != 0;
        return true;
    case Number::Unsigned:
        *static_cast<bool *>(p) = n.u != 0;
        return true;
    case Number::Real:
        *static_cast<bool *>(p) = !qFuzzyIsNull(n.d);
        return true;
    }
    return false;
}

bool storeNumber(int id, void *p, const Number &n) {
    switch (id) {
    case QMetaType::Bool:
        return storeBool(p, n);
    case QMetaType::Int:
        return storeIntegral<int>(p, n);
    case QMetaType::UInt:
        return storeIntegral<unsigned>(p, n);
    case QMetaType::LongLong:
        return storeIntegral<long long>(p, n);
    case QMetaType::ULongLong:
        return storeIntegral<unsigned long long>(p, n);
    case QMetaType::Double:
        return storeReal<double>(p, n);
    case QMetaType::Long:
        return storeIntegral<long>(p, n);
    case QMetaType::Short:
        return storeIntegral<short>(p, n);
    case QMetaType::Char:
        return storeIntegral<char>(p, n);
    case QMetaType::ULong:
        return storeIntegral<unsigned long>(p, n);
    case QMetaType::UShort:
        return storeIntegral<unsigned short>(p, n);
    case QMetaType::UChar:
        return storeIntegral<unsigned char>(p, n);
    case QMetaType::Float:
        return storeReal<float>(p, n);
    case QMetaType::SChar:
        return storeIntegral<signed char>(p, n);
    case QMetaType::Char16:
        return storeIntegral<char16_t>(p, n);
    case QMetaType::Char32:
        return storeIntegral<char32_t>(p, n);
    default:
        return false;
    }
}

bool parseNumber(const QString &s, Number &n) {
    bool ok = false;
    const QString t = s.trimmed();
    const qlonglong ll = t.toLongLong(&ok);
    if (ok) {
        n.kind = Number::Signed;
        n.s = ll;
        return true;
    }
    const qulonglong ull = t.toULongLong(&ok);
    if (ok) {
        n.kind = Number::Unsigned;
        n.u = ull;
        return true;
    }
    const double d = t.toDouble(&ok);
    if (ok) {
        n.kind = Number::Real;
        n.d = d;
        return true;
    }
    return false;
}

bool stringToBool(const QString &s) {
    const QString t = s.trimmed();
    return !(t.isEmpty() || t == QLatin1String("0") || t.toLower() == QLatin1String("false"));
}

QString numberToQString(const Number &n, int id) {
    switch (n.kind) {
    case Number::Signed:
        return QString::number(n.s);
    case Number::Unsigned:
        return QString::number(n.u);
    case Number::Real:
        return id == QMetaType::Float ? numberToString(static_cast<float>(n.d)) : numberToString(n.d);
    }
    return QString();
}

// The enumerator that describes an enumeration type, or an invalid one.
QMetaEnum enumeratorOf(QMetaType type) {
    const QMetaObject *mo = type.metaObject();
    if (!mo)
        return QMetaEnum();
    std::string name = type.iface()->enumName ? type.iface()->enumName() : (type.name() ? type.name() : "");
    const std::size_t colon = name.rfind("::");
    if (colon != std::string::npos)
        name.erase(0, colon + 2);
    const int idx = mo->indexOfEnumerator(name.c_str());
    return idx < 0 ? QMetaEnum() : mo->enumerator(idx);
}

bool readEnum(QMetaType type, const void *p, qlonglong &value) {
    const bool isUnsigned = type.iface()->flags & kFlagIsUnsignedEnumeration;
    switch (type.sizeOf()) {
    case 1:
        value = isUnsigned ? static_cast<qlonglong>(*static_cast<const unsigned char *>(p))
                           : static_cast<qlonglong>(*static_cast<const signed char *>(p));
        return true;
    case 2:
        value = isUnsigned ? static_cast<qlonglong>(*static_cast<const unsigned short *>(p))
                           : static_cast<qlonglong>(*static_cast<const short *>(p));
        return true;
    case 4:
        value = isUnsigned ? static_cast<qlonglong>(*static_cast<const unsigned *>(p))
                           : static_cast<qlonglong>(*static_cast<const int *>(p));
        return true;
    case 8:
        value = *static_cast<const long long *>(p);
        return true;
    default:
        return false;
    }
}

bool writeEnum(QMetaType type, void *p, qlonglong value) {
    switch (type.sizeOf()) {
    case 1:
        *static_cast<signed char *>(p) = static_cast<signed char>(value);
        return true;
    case 2:
        *static_cast<short *>(p) = static_cast<short>(value);
        return true;
    case 4:
        *static_cast<int *>(p) = static_cast<int>(value);
        return true;
    case 8:
        *static_cast<long long *>(p) = value;
        return true;
    default:
        return false;
    }
}

bool isQVersionNumber(QMetaType t) { return t == QMetaType::fromType<QVersionNumber>(); }

bool isStringLike(int id) { return id == QMetaType::QString || id == QMetaType::QByteArray; }

QString stringOf(int id, const void *p) {
    return id == QMetaType::QString ? *static_cast<const QString *>(p)
                                    : QString::fromUtf8(*static_cast<const QByteArray *>(p));
}

// Converts each element of a sequence into a QVariant.
bool sequenceToVariants(QMetaType from, const void *src, QVariantList &out) {
    const SequenceInterface *seq = from.iface()->sequence;
    if (!seq)
        return false;
    const QMetaType valueType(seq->valueType());
    const qsizetype n = seq->size(src);
    for (qsizetype i = 0; i < n; ++i) {
        void *tmp = allocateValue(valueType.iface());
        seq->valueAt(src, i, tmp);
        out.append(QVariant(valueType, tmp));
        valueType.destruct(tmp);
        freeValue(valueType.iface(), tmp);
    }
    return true;
}

bool convertContainers(QMetaType from, const void *src, QMetaType to, void *dst) {
    const int toId = to.id();
    if (toId == QMetaType::QVariantList || toId == QMetaType::QStringList || toId == QMetaType::QByteArrayList) {
        QVariantList list;
        if (from.id() == QMetaType::QString && toId == QMetaType::QStringList) {
            static_cast<QStringList *>(dst)->clear();
            static_cast<QStringList *>(dst)->append(*static_cast<const QString *>(src));
            return true;
        }
        if (!sequenceToVariants(from, src, list))
            return false;
        if (toId == QMetaType::QVariantList) {
            *static_cast<QVariantList *>(dst) = list;
            return true;
        }
        if (toId == QMetaType::QStringList) {
            QStringList r;
            for (const QVariant &v : list) {
                if (!v.canConvert(QMetaType::fromType<QString>()))
                    return false;
                r.append(v.toString());
            }
            *static_cast<QStringList *>(dst) = r;
            return true;
        }
        QByteArrayList r;
        for (const QVariant &v : list) {
            if (!v.canConvert(QMetaType::fromType<QByteArray>()))
                return false;
            r.append(v.toByteArray());
        }
        *static_cast<QByteArrayList *>(dst) = r;
        return true;
    }
    if (toId == QMetaType::QVariantMap || toId == QMetaType::QVariantHash) {
        const AssociativeInterface *assoc = from.iface()->associative;
        if (!assoc)
            return false;
        const QMetaType keyType(assoc->keyType());
        const QMetaType mappedType(assoc->mappedType());
        const qsizetype n = assoc->size(src);
        QVariantMap map;
        QVariantHash hash;
        for (qsizetype i = 0; i < n; ++i) {
            void *k = allocateValue(keyType.iface());
            void *m = allocateValue(mappedType.iface());
            assoc->keyAt(src, i, k);
            assoc->mappedAt(src, i, m);
            const QVariant key(keyType, k);
            const QVariant mapped(mappedType, m);
            keyType.destruct(k);
            mappedType.destruct(m);
            freeValue(keyType.iface(), k);
            freeValue(mappedType.iface(), m);
            if (!key.canConvert(QMetaType::fromType<QString>()))
                return false;
            if (toId == QMetaType::QVariantMap)
                map.insert(key.toString(), mapped);
            else
                hash.insert(key.toString(), mapped);
        }
        if (toId == QMetaType::QVariantMap)
            *static_cast<QVariantMap *>(dst) = map;
        else
            *static_cast<QVariantHash *>(dst) = hash;
        return true;
    }
    return false;
}

// Converts the elements of a container, one by one, into those of another container.
bool convertToContainer(QMetaType from, const void *src, QMetaType to, void *dst) {
    if (const SequenceInterface *seq = to.iface()->sequence; seq && seq->append && from.iface()->sequence) {
        QVariantList list;
        if (!sequenceToVariants(from, src, list))
            return false;
        const QMetaType valueType(seq->valueType());
        seq->clear(dst);
        for (const QVariant &element : list) {
            QVariant v = element;
            if (v.metaType() != valueType && !v.convert(valueType))
                return false;
            seq->append(dst, v.constData());
        }
        return true;
    }
    const AssociativeInterface *to_assoc = to.iface()->associative;
    const AssociativeInterface *from_assoc = from.iface()->associative;
    if (!to_assoc || !to_assoc->insert || !from_assoc)
        return false;
    const QMetaType keyType(to_assoc->keyType());
    const QMetaType mappedType(to_assoc->mappedType());
    to_assoc->clear(dst);
    const qsizetype n = from_assoc->size(src);
    for (qsizetype i = 0; i < n; ++i) {
        const QMetaType fromKey(from_assoc->keyType());
        const QMetaType fromMapped(from_assoc->mappedType());
        void *k = allocateValue(fromKey.iface());
        void *m = allocateValue(fromMapped.iface());
        from_assoc->keyAt(src, i, k);
        from_assoc->mappedAt(src, i, m);
        QVariant key(fromKey, k);
        QVariant mapped(fromMapped, m);
        fromKey.destruct(k);
        fromMapped.destruct(m);
        freeValue(fromKey.iface(), k);
        freeValue(fromMapped.iface(), m);
        if ((key.metaType() != keyType && !key.convert(keyType)) ||
            (mapped.metaType() != mappedType && !mapped.convert(mappedType)))
            return false;
        to_assoc->insert(dst, key.constData(), mapped.constData());
    }
    return true;
}

bool containerTarget(int toId) {
    return toId == QMetaType::QVariantList || toId == QMetaType::QStringList || toId == QMetaType::QByteArrayList ||
           toId == QMetaType::QVariantMap || toId == QMetaType::QVariantHash;
}

} // namespace

bool canConvertBuiltin(QMetaType from, QMetaType to) {
    const int f = from.id();
    const int t = to.id();
    const bool fromNum = isNumericId(f);
    const bool toNum = isNumericId(t);
    if (isEnumType(from)) {
        if (toNum || t == QMetaType::QChar)
            return true;
        if (isEnumType(to))
            return true;
        return (t == QMetaType::QString || t == QMetaType::QByteArray) && enumeratorOf(from).isValid();
    }
    if (isEnumType(to))
        return fromNum || f == QMetaType::QChar || (isStringLike(f) && enumeratorOf(to).isValid());
    if (toNum)
        return fromNum || f == QMetaType::QChar || isStringLike(f);
    switch (t) {
    case QMetaType::QChar:
        return fromNum || f == QMetaType::QString;
    case QMetaType::QString:
        return fromNum || f == QMetaType::QChar || f == QMetaType::QByteArray || f == QMetaType::QDate ||
               f == QMetaType::QTime || f == QMetaType::QDateTime || f == QMetaType::QUrl || f == QMetaType::QUuid ||
               isQVersionNumber(from) || f == QMetaType::QStringList;
    case QMetaType::QByteArray:
        return fromNum || f == QMetaType::QString || f == QMetaType::QUuid;
    case QMetaType::QDate:
    case QMetaType::QTime:
        return f == QMetaType::QString || f == QMetaType::QDateTime;
    case QMetaType::QDateTime:
        return f == QMetaType::QString || f == QMetaType::QDate;
    case QMetaType::QUrl:
    case QMetaType::QUuid:
        return f == QMetaType::QString;
    case QMetaType::QPoint:
        return f == QMetaType::QPointF;
    case QMetaType::QPointF:
        return f == QMetaType::QPoint;
    case QMetaType::QSize:
        return f == QMetaType::QSizeF;
    case QMetaType::QSizeF:
        return f == QMetaType::QSize;
    case QMetaType::QLine:
        return f == QMetaType::QLineF;
    case QMetaType::QLineF:
        return f == QMetaType::QLine;
    case QMetaType::QRect:
        return f == QMetaType::QRectF;
    case QMetaType::QRectF:
        return f == QMetaType::QRect;
    case QMetaType::QObjectStar:
        return from.flags().testFlag(QMetaType::PointerToQObject) || f == QMetaType::QObjectStar;
    case QMetaType::QVariantList:
    case QMetaType::QStringList:
    case QMetaType::QByteArrayList:
        return (from.iface() && from.iface()->sequence) || (t == QMetaType::QStringList && f == QMetaType::QString);
    case QMetaType::QVariantMap:
    case QMetaType::QVariantHash:
        return from.iface() && from.iface()->associative;
    default:
        break;
    }
    if (isQVersionNumber(to))
        return f == QMetaType::QString;
    if (to.flags().testFlag(QMetaType::PointerToQObject))
        return from.flags().testFlag(QMetaType::PointerToQObject) || f == QMetaType::QObjectStar;
    if (to.iface()->sequence && to.iface()->sequence->append && from.iface()->sequence)
        return true;
    return to.iface()->associative && to.iface()->associative->insert && from.iface()->associative;
}

bool convertBuiltin(QMetaType from, const void *src, QMetaType to, void *dst) {
    const int f = from.id();
    const int t = to.id();
    Number num;

    // Enumerations
    if (isEnumType(from)) {
        qlonglong value = 0;
        if (!readEnum(from, src, value))
            return false;
        if (isEnumType(to))
            return writeEnum(to, dst, value);
        if (isNumericId(t)) {
            num.kind = Number::Signed;
            num.s = value;
            return storeNumber(t, dst, num);
        }
        if (t == QMetaType::QChar) {
            *static_cast<QChar *>(dst) = QChar(static_cast<char16_t>(value));
            return true;
        }
        if (t == QMetaType::QString || t == QMetaType::QByteArray) {
            const QMetaEnum e = enumeratorOf(from);
            if (!e.isValid())
                return false;
            QByteArray key;
            if (e.isFlag()) {
                key = e.valueToKeys(static_cast<int>(value));
            } else if (const char *k = e.valueToKey(static_cast<int>(value))) {
                key = k;
            } else {
                return false;
            }
            if (t == QMetaType::QString)
                *static_cast<QString *>(dst) = QString::fromUtf8(key);
            else
                *static_cast<QByteArray *>(dst) = key;
            return true;
        }
        return false;
    }
    if (isEnumType(to)) {
        qlonglong value = 0;
        if (readNumber(f, src, num)) {
            if (num.kind == Number::Signed)
                value = num.s;
            else if (num.kind == Number::Unsigned)
                value = static_cast<qlonglong>(num.u);
            else
                value = static_cast<qlonglong>(std::llround(num.d));
            return writeEnum(to, dst, value);
        }
        if (f == QMetaType::QChar) {
            return writeEnum(to, dst, static_cast<qlonglong>(static_cast<const QChar *>(src)->unicode()));
        }
        if (isStringLike(f)) {
            const QString s = stringOf(f, src);
            const QMetaEnum e = enumeratorOf(to);
            bool ok = false;
            if (e.isValid()) {
                const QByteArray k = s.toUtf8();
                const int v = e.isFlag() ? e.keysToValue(k.constData(), &ok) : e.keyToValue(k.constData(), &ok);
                if (ok)
                    return writeEnum(to, dst, v);
            }
            if (parseNumber(s, num) && num.kind != Number::Real)
                return writeEnum(to, dst, num.kind == Number::Signed ? num.s : static_cast<qlonglong>(num.u));
        }
        return false;
    }

    // Numbers, bool and QChar
    if (isNumericId(t)) {
        if (readNumber(f, src, num))
            return storeNumber(t, dst, num);
        if (f == QMetaType::QChar) {
            num.kind = Number::Unsigned;
            num.u = static_cast<const QChar *>(src)->unicode();
            return storeNumber(t, dst, num);
        }
        if (isStringLike(f)) {
            const QString s = stringOf(f, src);
            if (t == QMetaType::Bool) {
                *static_cast<bool *>(dst) = stringToBool(s);
                return true;
            }
            return parseNumber(s, num) && storeNumber(t, dst, num);
        }
        return false;
    }

    switch (t) {
    case QMetaType::QChar: {
        if (readNumber(f, src, num)) {
            qulonglong v = num.kind == Number::Signed
                               ? static_cast<qulonglong>(num.s)
                               : (num.kind == Number::Unsigned ? num.u : static_cast<qulonglong>(num.d));
            if (v > 0xffff)
                return false;
            *static_cast<QChar *>(dst) = QChar(static_cast<char16_t>(v));
            return true;
        }
        if (f == QMetaType::QString) {
            const QString &s = *static_cast<const QString *>(src);
            if (s.size() != 1)
                return false;
            *static_cast<QChar *>(dst) = s.at(0);
            return true;
        }
        return false;
    }
    case QMetaType::QString: {
        QString &out = *static_cast<QString *>(dst);
        if (readNumber(f, src, num)) {
            out = f == QMetaType::Bool ? QString::fromLatin1(num.u ? "true" : "false") : numberToQString(num, f);
            return true;
        }
        switch (f) {
        case QMetaType::QChar:
            out = QString(*static_cast<const QChar *>(src));
            return true;
        case QMetaType::QByteArray:
            out = QString::fromUtf8(*static_cast<const QByteArray *>(src));
            return true;
        case QMetaType::QDate:
            out = static_cast<const QDate *>(src)->toString(Qt::ISODate);
            return true;
        case QMetaType::QTime:
            out = static_cast<const QTime *>(src)->toString(Qt::ISODateWithMs);
            return true;
        case QMetaType::QDateTime:
            out = static_cast<const QDateTime *>(src)->toString(Qt::ISODateWithMs);
            return true;
        case QMetaType::QUrl:
            out = static_cast<const QUrl *>(src)->toString();
            return true;
        case QMetaType::QUuid:
            out = static_cast<const QUuid *>(src)->toString();
            return true;
        case QMetaType::QStringList: {
            const QStringList &l = *static_cast<const QStringList *>(src);
            if (l.size() != 1)
                return false;
            out = l.at(0);
            return true;
        }
        default:
            break;
        }
        if (isQVersionNumber(from)) {
            out = static_cast<const QVersionNumber *>(src)->toString();
            return true;
        }
        return false;
    }
    case QMetaType::QByteArray: {
        QByteArray &out = *static_cast<QByteArray *>(dst);
        if (readNumber(f, src, num)) {
            out = (f == QMetaType::Bool ? QString::fromLatin1(num.u ? "true" : "false") : numberToQString(num, f))
                      .toUtf8();
            return true;
        }
        if (f == QMetaType::QString) {
            out = static_cast<const QString *>(src)->toUtf8();
            return true;
        }
        if (f == QMetaType::QUuid) {
            out = static_cast<const QUuid *>(src)->toByteArray();
            return true;
        }
        return false;
    }
    case QMetaType::QDate:
        if (f == QMetaType::QString) {
            const QDate d = QDate::fromString(*static_cast<const QString *>(src), Qt::ISODate);
            if (!d.isValid())
                return false;
            *static_cast<QDate *>(dst) = d;
            return true;
        }
        if (f == QMetaType::QDateTime) {
            *static_cast<QDate *>(dst) = static_cast<const QDateTime *>(src)->date();
            return true;
        }
        return false;
    case QMetaType::QTime:
        if (f == QMetaType::QString) {
            const QTime v = QTime::fromString(*static_cast<const QString *>(src), Qt::ISODate);
            if (!v.isValid())
                return false;
            *static_cast<QTime *>(dst) = v;
            return true;
        }
        if (f == QMetaType::QDateTime) {
            *static_cast<QTime *>(dst) = static_cast<const QDateTime *>(src)->time();
            return true;
        }
        return false;
    case QMetaType::QDateTime:
        if (f == QMetaType::QString) {
            const QDateTime v = QDateTime::fromString(*static_cast<const QString *>(src), Qt::ISODate);
            if (!v.isValid())
                return false;
            *static_cast<QDateTime *>(dst) = v;
            return true;
        }
        if (f == QMetaType::QDate) {
            *static_cast<QDateTime *>(dst) = QDateTime(*static_cast<const QDate *>(src), QTime(0, 0));
            return true;
        }
        return false;
    case QMetaType::QUrl:
        if (f == QMetaType::QString) {
            *static_cast<QUrl *>(dst) = QUrl(*static_cast<const QString *>(src));
            return true;
        }
        return false;
    case QMetaType::QUuid:
        if (f == QMetaType::QString) {
            const QUuid u = QUuid::fromString(QStringView(*static_cast<const QString *>(src)));
            if (u.isNull() &&
                static_cast<const QString *>(src)->trimmed().remove(QLatin1Char('{')).remove(QLatin1Char('}')) !=
                    QStringLiteral("00000000-0000-0000-0000-000000000000"))
                return false;
            *static_cast<QUuid *>(dst) = u;
            return true;
        }
        return false;
    case QMetaType::QPoint:
        if (f == QMetaType::QPointF) {
            *static_cast<QPoint *>(dst) = static_cast<const QPointF *>(src)->toPoint();
            return true;
        }
        return false;
    case QMetaType::QPointF:
        if (f == QMetaType::QPoint) {
            *static_cast<QPointF *>(dst) = QPointF(*static_cast<const QPoint *>(src));
            return true;
        }
        return false;
    case QMetaType::QSize:
        if (f == QMetaType::QSizeF) {
            *static_cast<QSize *>(dst) = static_cast<const QSizeF *>(src)->toSize();
            return true;
        }
        return false;
    case QMetaType::QSizeF:
        if (f == QMetaType::QSize) {
            *static_cast<QSizeF *>(dst) = static_cast<const QSize *>(src)->toSizeF();
            return true;
        }
        return false;
    case QMetaType::QLine:
        if (f == QMetaType::QLineF) {
            *static_cast<QLine *>(dst) = static_cast<const QLineF *>(src)->toLine();
            return true;
        }
        return false;
    case QMetaType::QLineF:
        if (f == QMetaType::QLine) {
            *static_cast<QLineF *>(dst) = QLineF(*static_cast<const QLine *>(src));
            return true;
        }
        return false;
    case QMetaType::QRect:
        if (f == QMetaType::QRectF) {
            *static_cast<QRect *>(dst) = static_cast<const QRectF *>(src)->toRect();
            return true;
        }
        return false;
    case QMetaType::QRectF:
        if (f == QMetaType::QRect) {
            *static_cast<QRectF *>(dst) = QRectF(*static_cast<const QRect *>(src));
            return true;
        }
        return false;
    case QMetaType::QObjectStar:
    default:
        break;
    }

    if (isQVersionNumber(to)) {
        if (f != QMetaType::QString)
            return false;
        const QVersionNumber v = QVersionNumber::fromString(QStringView(*static_cast<const QString *>(src)));
        if (v.isNull())
            return false;
        *static_cast<QVersionNumber *>(dst) = v;
        return true;
    }

    // Pointers to QObject-derived classes
    if (to.flags().testFlag(QMetaType::PointerToQObject) || t == QMetaType::QObjectStar) {
        if (!(from.flags().testFlag(QMetaType::PointerToQObject) || f == QMetaType::QObjectStar))
            return false;
        QObject *obj = *static_cast<QObject *const *>(src);
        if (obj && t != QMetaType::QObjectStar) {
            const QMetaObject *target = to.metaObject();
            if (!target || !obj->metaObject()->inherits(target))
                return false;
        }
        *static_cast<QObject **>(dst) = obj;
        return true;
    }

    if (containerTarget(t))
        return convertContainers(from, src, to, dst);
    return convertToContainer(from, src, to, dst);
}

} // namespace QtPrivate
