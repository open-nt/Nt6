// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qcborcommon.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>

#include <cstddef>
#include <memory>
#include <type_traits>

class QCborArray;
class QCborMap;
class QCborStreamReader;
class QCborStreamWriter;
class QCborValueConstRef;
class QCborValueRef;
class QJsonValue;
class QVariant;

namespace QtPrivate {
struct CborRefAccess;
} // namespace QtPrivate

/*!
    \class QCborParserError
    \brief The QCborParserError class is used by QCborValue to report a parsing error.
    \since 5.12

    QCborValue::fromCbor() fills it in when the data is not valid CBOR: the error, and the offset
    in bytes where it was found.

    \differences
    None.
    \enddifferences

    \sa QCborValue::fromCbor()
*/
struct QCborParserError {
    /*!
        The offset in bytes at which the error was found.
    */
    qint64 offset = 0;
    /*!
        The error that was found, or QCborError::NoError.
    */
    QCborError error;

    /*!
        Returns a text for the error.

        \sa QCborError::toString()
    */
    QString errorString() const { return error.toString(); }
};

class QCborValue {
public:
    /*!
        \enum QCborValue::EncodingOption

        The options of toCbor(). They may be combined.

        \value NoTransformation Write each value as it is.
        \value SortKeysInMaps Write the pairs of every map ordered by their encoded keys, shorter first, as the canonical encoding of RFC 7049 does. Use it to get the same bytes for equal maps.
        \value UseFloat Write a floating-point number in 32 bits when that loses nothing.
        \value UseFloat16 Write a floating-point number in 16 bits, or else in 32, when that loses nothing.
        \value UseIntegers Write a floating-point number that is a whole number as an integer.
    */
    enum EncodingOption {
        SortKeysInMaps = 0x01,
        UseFloat = 0x02,
        UseFloat16 = UseFloat | 0x04,
        UseIntegers = 0x08,
        NoTransformation = 0,
    };
    Q_DECLARE_FLAGS(EncodingOptions, EncodingOption)

    /*!
        \enum QCborValue::DiagnosticNotationOption

        The options of toDiagnosticNotation().

        \value Compact Everything on one line.
        \value LineWrapped Each element of an array and each pair of a map on a line of its own, indented by its depth.
        \value ExtendedFormat Show more of the encoding: the tag of an extended type, such as \c{0("...")} for a date and time, and the size of a floating-point number.
    */
    enum DiagnosticNotationOption { Compact = 0x0, LineWrapped = 0x01, ExtendedFormat = 0x02 };
    Q_DECLARE_FLAGS(DiagnosticNotationOptions, DiagnosticNotationOption)

    /*!
        \enum QCborValue::Type

        The types of value. The extended types are tags of CBOR that QCborValue knows: it
        converts their contents to the Nt type.

        \value Integer An integer of 64 bits.
        \value ByteArray A byte array.
        \value String A string.
        \value Array A QCborArray.
        \value Map A QCborMap.
        \value Tag A tag and the value it applies to.
        \value SimpleType A simple type that has no meaning of its own.
        \value False The value \c false.
        \value True The value \c true.
        \value Null The value \c null.
        \value Undefined The value \c undefined, which is what a default-constructed QCborValue holds.
        \value Double A floating-point number.
        \value DateTime A QDateTime, as the tag QCborKnownTags::DateTimeString.
        \value Url A QUrl, as the tag QCborKnownTags::Url.
        \value RegularExpression A QRegularExpression, as the tag QCborKnownTags::RegularExpression.
        \value Uuid A QUuid, as the tag QCborKnownTags::Uuid.
        \value Invalid Not a value: what an operation that fails returns.
    */
    enum Type : int {
        Integer = 0x00,
        ByteArray = 0x40,
        String = 0x60,
        Array = 0x80,
        Map = 0xa0,
        Tag = 0xc0,
        SimpleType = 0x100,
        False = SimpleType + int(QCborSimpleType::False),
        True = SimpleType + int(QCborSimpleType::True),
        Null = SimpleType + int(QCborSimpleType::Null),
        Undefined = SimpleType + int(QCborSimpleType::Undefined),
        Double = 0x202,
        DateTime = 0x10000,
        Url = 0x10020,
        RegularExpression = 0x10023,
        Uuid = 0x10025,
        Invalid = -1,
    };

    QCborValue() noexcept;
    QCborValue(Type t_);
    QCborValue(std::nullptr_t);
    QCborValue(bool b);
    QCborValue(int i);
    QCborValue(unsigned u);
    QCborValue(qint64 i);
    /*!
        \overload
        Constructs an integer from \a i, where \c{long long} is another type than \c qint64.
    */
    template <typename T,
              std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, bool> = true>
    QCborValue(T i) : QCborValue(static_cast<qint64>(i)) {}
    QCborValue(double v);
    QCborValue(QCborSimpleType st);
    QCborValue(const QByteArray &ba);
    QCborValue(const QString &s);
    QCborValue(QStringView s);
    QCborValue(QLatin1StringView s);
    QCborValue(const char *s);
    QCborValue(const QCborArray &a);
    QCborValue(const QCborMap &m);
    QCborValue(QCborTag tag, const QCborValue &taggedValue = QCborValue());
    QCborValue(QCborKnownTags tag, const QCborValue &taggedValue = QCborValue());
    explicit QCborValue(const QDateTime &dt);
    explicit QCborValue(const QUrl &url);
    explicit QCborValue(const QRegularExpression &rx);
    explicit QCborValue(const QUuid &uuid);
    ~QCborValue();
    QCborValue(const QCborValue &other);
    QCborValue(QCborValue &&other) noexcept;
    QCborValue &operator=(const QCborValue &other);
    QCborValue &operator=(QCborValue &&other) noexcept;

    void swap(QCborValue &other) noexcept;

    /*!
        Returns the type of the value.
    */
    Type type() const { return t; }
    /*!
        Returns \c true if the value is an integer.
    */
    bool isInteger() const { return t == Integer; }
    /*!
        Returns \c true if the value is a byte array.
    */
    bool isByteArray() const { return t == ByteArray; }
    /*!
        Returns \c true if the value is a string.
    */
    bool isString() const { return t == String; }
    /*!
        Returns \c true if the value is an array.
    */
    bool isArray() const { return t == Array; }
    /*!
        Returns \c true if the value is a map.
    */
    bool isMap() const { return t == Map; }
    /*!
        Returns \c true if the value is a tag, also one of the extended types, which are tags too.

        \sa tag(), taggedValue()
    */
    bool isTag() const { return t == Tag || t >= DateTime; }
    /*!
        Returns \c true if the value is \c false.
    */
    bool isFalse() const { return t == False; }
    /*!
        Returns \c true if the value is \c true.
    */
    bool isTrue() const { return t == True; }
    /*!
        Returns \c true if the value is \c false or \c true.
    */
    bool isBool() const { return t == False || t == True; }
    /*!
        Returns \c true if the value is \c null.
    */
    bool isNull() const { return t == Null; }
    /*!
        Returns \c true if the value is \c undefined.
    */
    bool isUndefined() const { return t == Undefined; }
    /*!
        Returns \c true if the value is a floating-point number.
    */
    bool isDouble() const { return t == Double; }
    /*!
        Returns \c true if the value is a date and time.
    */
    bool isDateTime() const { return t == DateTime; }
    /*!
        Returns \c true if the value is a URL.
    */
    bool isUrl() const { return t == Url; }
    /*!
        Returns \c true if the value is a regular expression.
    */
    bool isRegularExpression() const { return t == RegularExpression; }
    /*!
        Returns \c true if the value is a UUID.
    */
    bool isUuid() const { return t == Uuid; }
    /*!
        Returns \c true if the value is invalid.
    */
    bool isInvalid() const { return t == Invalid; }
    /*!
        Returns \c true if the value is an array or a map.
    */
    bool isContainer() const { return isArray() || isMap(); }
    /*!
        Returns \c true if the value is a simple type, among them \c false, \c true, \c null and
        \c undefined.
    */
    bool isSimpleType() const { return t >= SimpleType && t < Double; }
    /*!
        \overload
        Returns \c true if the value is the simple type \a st.
    */
    bool isSimpleType(QCborSimpleType st) const { return t == static_cast<Type>(SimpleType + int(st)); }

    QCborSimpleType toSimpleType(QCborSimpleType defaultValue = QCborSimpleType::Undefined) const;
    qint64 toInteger(qint64 defaultValue = 0) const;
    bool toBool(bool defaultValue = false) const;
    double toDouble(double defaultValue = 0) const;
    QCborTag tag(QCborTag defaultValue = QCborTag(~0ULL)) const;
    QCborValue taggedValue(const QCborValue &defaultValue = QCborValue()) const;
    QByteArray toByteArray(const QByteArray &defaultValue = {}) const;
    QString toString(const QString &defaultValue = {}) const;
    QAnyStringView toStringView(QAnyStringView defaultValue = {}) const;
    QDateTime toDateTime(const QDateTime &defaultValue = {}) const;
    QUrl toUrl(const QUrl &defaultValue = {}) const;
    QRegularExpression toRegularExpression(const QRegularExpression &defaultValue = {}) const;
    QUuid toUuid(const QUuid &defaultValue = {}) const;
    QCborArray toArray() const;
    QCborArray toArray(const QCborArray &defaultValue) const;
    QCborMap toMap() const;
    QCborMap toMap(const QCborMap &defaultValue) const;

    const QCborValue operator[](const QString &key) const;
    const QCborValue operator[](QLatin1StringView key) const;
    const QCborValue operator[](qint64 key) const;
    QCborValueRef operator[](qint64 key);
    QCborValueRef operator[](QLatin1StringView key);
    QCborValueRef operator[](const QString &key);

    int compare(const QCborValue &other) const;
    /*!
        Returns \c true if \a a and \a b are of the same type and hold the same value.

        \sa compare()
    */
    friend bool operator==(const QCborValue &a, const QCborValue &b) { return a.compare(b) == 0; }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QCborValue &a, const QCborValue &b) { return a.compare(b) != 0; }
    /*!
        Returns \c true if \a a comes before \a b in the order of compare().
    */
    friend bool operator<(const QCborValue &a, const QCborValue &b) { return a.compare(b) < 0; }
    /*!
        Returns \c true if \a a comes before \a b in the order of compare(), or is equal to it.
    */
    friend bool operator<=(const QCborValue &a, const QCborValue &b) { return a.compare(b) <= 0; }
    /*!
        Returns \c true if \a a comes after \a b in the order of compare().
    */
    friend bool operator>(const QCborValue &a, const QCborValue &b) { return a.compare(b) > 0; }
    /*!
        Returns \c true if \a a comes after \a b in the order of compare(), or is equal to it.
    */
    friend bool operator>=(const QCborValue &a, const QCborValue &b) { return a.compare(b) >= 0; }

    static QCborValue fromVariant(const QVariant &variant);
    QVariant toVariant() const;
    static QCborValue fromJsonValue(const QJsonValue &v);
    QJsonValue toJsonValue() const;

    static QCborValue fromCbor(QCborStreamReader &reader);
    static QCborValue fromCbor(const QByteArray &ba, QCborParserError *error = nullptr);
    static QCborValue fromCbor(const char *data, qsizetype len, QCborParserError *error = nullptr);
    static QCborValue fromCbor(const quint8 *data, qsizetype len, QCborParserError *error = nullptr);
    QByteArray toCbor(EncodingOptions opt = NoTransformation) const;
    void toCbor(QCborStreamWriter &writer, EncodingOptions opt = NoTransformation) const;

    QString toDiagnosticNotation(DiagnosticNotationOptions opts = Compact) const;

private:
    friend class QCborArray;
    friend class QCborMap;
    friend class QCborValueConstRef;
    friend class QCborValueRef;
    struct Payload;
    Payload &payload();
    const Payload *constPayload() const { return d.get(); }

    Type t = Undefined;
    qint64 n = 0;               // the integer, or the number of a tag
    double dbl = 0;             // the floating-point number
    std::shared_ptr<Payload> d; // the bytes, string, container or tagged value; shared until it is changed
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCborValue::EncodingOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCborValue::DiagnosticNotationOptions)

class QCborValueConstRef {
public:
    /*!
        Constructs a reference to the value that \a other refers to.
    */
    QCborValueConstRef(const QCborValueConstRef &other) = default;
    QCborValueConstRef &operator=(const QCborValueConstRef &) = delete;

    /*!
        Returns a copy of the value that the reference refers to.

        \sa concrete()
    */
    operator QCborValue() const { return *v; }
    /*!
        Returns a copy of the value that the reference refers to.
    */
    QCborValue concrete() const { return *v; }

    QCborValue::Type type() const;
    bool isInteger() const;
    bool isByteArray() const;
    bool isString() const;
    bool isArray() const;
    bool isMap() const;
    bool isTag() const;
    bool isFalse() const;
    bool isTrue() const;
    bool isBool() const;
    bool isNull() const;
    bool isUndefined() const;
    bool isDouble() const;
    bool isDateTime() const;
    bool isUrl() const;
    bool isRegularExpression() const;
    bool isUuid() const;
    bool isInvalid() const;
    bool isContainer() const;
    bool isSimpleType() const;
    bool isSimpleType(QCborSimpleType st) const;
    QCborSimpleType toSimpleType(QCborSimpleType defaultValue = QCborSimpleType::Undefined) const;
    qint64 toInteger(qint64 defaultValue = 0) const;
    bool toBool(bool defaultValue = false) const;
    double toDouble(double defaultValue = 0) const;
    QCborTag tag(QCborTag defaultValue = QCborTag(~0ULL)) const;
    QCborValue taggedValue(const QCborValue &defaultValue = QCborValue()) const;
    QByteArray toByteArray(const QByteArray &defaultValue = {}) const;
    QString toString(const QString &defaultValue = {}) const;
    QAnyStringView toStringView(QAnyStringView defaultValue = {}) const;
    QDateTime toDateTime(const QDateTime &defaultValue = {}) const;
    QUrl toUrl(const QUrl &defaultValue = {}) const;
    QRegularExpression toRegularExpression(const QRegularExpression &defaultValue = {}) const;
    QUuid toUuid(const QUuid &defaultValue = {}) const;
    QCborArray toArray() const;
    QCborArray toArray(const QCborArray &defaultValue) const;
    QCborMap toMap() const;
    QCborMap toMap(const QCborMap &defaultValue) const;
    QVariant toVariant() const;
    QJsonValue toJsonValue() const;
    QByteArray toCbor(QCborValue::EncodingOptions opt = QCborValue::NoTransformation) const;
    void toCbor(QCborStreamWriter &writer, QCborValue::EncodingOptions opt = QCborValue::NoTransformation) const;
    QString toDiagnosticNotation(QCborValue::DiagnosticNotationOptions opt = QCborValue::Compact) const;
    int compare(const QCborValue &other) const;

    const QCborValue operator[](const QString &key) const;
    const QCborValue operator[](QLatin1StringView key) const;
    const QCborValue operator[](qint64 key) const;

    /*!
        Returns \c true if \a a refers to a value that is equal to \a b.
    */
    friend bool operator==(const QCborValueConstRef &a, const QCborValue &b) { return a.compare(b) == 0; }
    /*!
        Returns \c true if \a a refers to a value that differs from \a b.
    */
    friend bool operator!=(const QCborValueConstRef &a, const QCborValue &b) { return a.compare(b) != 0; }
    /*!
        Returns \c true if the value that \a a refers to comes before \a b.
    */
    friend bool operator<(const QCborValueConstRef &a, const QCborValue &b) { return a.compare(b) < 0; }

private:
    friend class QCborArray;
    friend class QCborMap;
    friend class QCborValue;
    friend class QCborValueRef;
    friend struct QtPrivate::CborRefAccess;
    explicit QCborValueConstRef(const QCborValue *value) : v(const_cast<QCborValue *>(value)) {}
    QCborValue *v = nullptr;
};

class QCborValueRef : public QCborValueConstRef {
public:
    /*!
        Constructs a reference to the value that \a other refers to.
    */
    QCborValueRef(const QCborValueRef &other) = default;
    QCborValueRef &operator=(const QCborValue &other);
    QCborValueRef &operator=(const QCborValueRef &other);

    using QCborValueConstRef::operator[];
    QCborValueRef operator[](qint64 key);
    QCborValueRef operator[](QLatin1StringView key);
    QCborValueRef operator[](const QString &key);

private:
    friend class QCborArray;
    friend class QCborMap;
    friend class QCborValue;
    friend struct QtPrivate::CborRefAccess;
    explicit QCborValueRef(QCborValue *value) : QCborValueConstRef(value) {}
};

namespace QtPrivate {

// Lets the iterators of the containers make references and move them.
struct CborRefAccess {
    static QCborValueRef ref(QCborValue *v) { return QCborValueRef(v); }
    static QCborValueConstRef constRef(const QCborValue *v) { return QCborValueConstRef(v); }
    static QCborValue *&pointer(QCborValueConstRef &r) { return r.v; }
    static QCborValue *pointer(const QCborValueConstRef &r) { return r.v; }
};

} // namespace QtPrivate
