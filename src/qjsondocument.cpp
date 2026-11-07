// MIT License - see LICENSE
#include <QtCore/qjsondocument.h>

#include <QtCore/qdebug.h>
#include <QtCore/qvariant.h>

#include "qjson_p.h"

/*!
    \class QJsonParseError
    \brief The QJsonParseError struct is used to report errors during JSON parsing.

    QJsonDocument::fromJson() fills one in when it is given a pointer to it.
    \c error is \c NoError on success; otherwise \c offset is the byte offset
    in the input at which the parser noticed the problem.

    \code
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text, &err);
    if (err.error != QJsonParseError::NoError)
        qWarning() << err.errorString() << "at" << err.offset;
    \endcode

    \differences
    The members start out as \c 0 and \c NoError instead of being
    uninitialized. The text of errorString() is not translated.
    \enddifferences

    \sa QJsonDocument::fromJson()
*/

/*!
    \enum QJsonParseError::ParseError

    The kinds of error the parser reports.

    \value NoError No error occurred
    \value UnterminatedObject An object is not correctly terminated with a closing curly bracket
    \value MissingNameSeparator A colon separating a name from its value is missing in an object
    \value UnterminatedArray An array is not correctly terminated with a closing square bracket
    \value MissingValueSeparator A comma separating different items is missing in an array or object
    \value IllegalValue The value is illegal
    \value TerminationByNumber The input stream ended while parsing a number
    \value IllegalNumber The number is not well formed
    \value IllegalEscapeSequence An illegal escape sequence occurred in the input
    \value IllegalUTF8String An illegal UTF-8 sequence occurred in the input
    \value UnterminatedString A string wasn't terminated with a quote
    \value MissingObject An object was expected but couldn't be found
    \value DeepNesting The JSON document is too deeply nested for the parser to parse it
    \value DocumentTooLarge The JSON document is too large for the parser to parse it
    \value GarbageAtEnd The parsed document contains additional garbage characters at the end
*/

/*!
    \variable QJsonParseError::offset

    The byte offset in the input at which the error was detected, or \c 0 if
    there was no error.
*/

/*!
    \variable QJsonParseError::error

    The type of the error, or \c NoError.
*/

/*!
    Returns a text description of \c error.
*/
QString QJsonParseError::errorString() const {
    const char *text = "";
    switch (error) {
    case NoError:
        text = "no error occurred";
        break;
    case UnterminatedObject:
        text = "unterminated object";
        break;
    case MissingNameSeparator:
        text = "missing name separator";
        break;
    case UnterminatedArray:
        text = "unterminated array";
        break;
    case MissingValueSeparator:
        text = "missing value separator";
        break;
    case IllegalValue:
        text = "illegal value";
        break;
    case TerminationByNumber:
        text = "invalid termination by number";
        break;
    case IllegalNumber:
        text = "illegal number";
        break;
    case IllegalEscapeSequence:
        text = "invalid escape sequence";
        break;
    case IllegalUTF8String:
        text = "invalid UTF8 string";
        break;
    case UnterminatedString:
        text = "unterminated string";
        break;
    case MissingObject:
        text = "object is missing after a comma";
        break;
    case DeepNesting:
        text = "too deeply nested document";
        break;
    case DocumentTooLarge:
        text = "too large document";
        break;
    case GarbageAtEnd:
        text = "garbage at the end of the document";
        break;
    }
    return QString::fromUtf8(text);
}

/*!
    \class QJsonDocument
    \brief The QJsonDocument class provides a way to read and write JSON documents.

    A QJsonDocument holds a complete JSON document, which is either an array
    or an object. It can be read from UTF-8 text with fromJson() and written
    back with toJson().

    \code
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson("{\"a\": [1, 2, 3]}", &err);
    if (doc.isObject())
        qDebug() << doc["a"][1].toInt();   // 2
    QByteArray text = doc.toJson(QJsonDocument::Compact);
    \endcode

    The parser follows RFC 8259: the text must be valid UTF-8, strings may use
    backslash-u escapes with four hex digits, including surrogate pairs, and there must be no
    trailing commas, comments or unquoted keys. A leading UTF-8 byte order mark
    is skipped. Documents nest at most 1024 levels deep. Integers that fit in
    2^53 are kept exact, other numbers become doubles, and a number
    that does not fit in a double is an error.

    A document written with Indented uses four spaces per level, puts each
    value on a line of its own and ends with a line feed; Compact writes no
    white space at all. Object keys appear in sorted order.

    \differences
    There is no \c fromRawData(), \c rawData() or \c fromBinaryData(), which Qt 6
    removed too. Top-level scalars are rejected as in Qt. A raw control
    character in a string is rejected as \c IllegalValue. A lone surrogate
    written with an escape is accepted and is written back as U+FFFD, the
    replacement character.
    \enddifferences

    \sa QJsonValue, QJsonObject, QJsonArray, QJsonParseError
*/

/*!
    \enum QJsonDocument::JsonFormat

    The formats toJson() can write.

    \value Indented Defines human readable output as follows:
    \code
    {
        "Array": [
            true,
            999
        ],
        "Key": "Value"
    }
    \endcode
    \value Compact Defines a compact output as follows:
    \code
    {"Array":[true,999],"Key":"Value"}
    \endcode
*/

/*!
    Creates a null document.

    \sa isNull()
*/
QJsonDocument::QJsonDocument() = default;

/*!
    Creates a document that holds \a object.
*/
QJsonDocument::QJsonDocument(const QJsonObject &object) : m_root(object) {}

/*!
    Creates a document that holds \a array.
*/
QJsonDocument::QJsonDocument(const QJsonArray &array) : m_root(array) {}

/*!
    Destroys the document.
*/
QJsonDocument::~QJsonDocument() = default;

/*!
    Creates a copy of \a other.
*/
QJsonDocument::QJsonDocument(const QJsonDocument &other) = default;

/*!
    Moves \a other into the new document, which leaves \a other null.
*/
QJsonDocument::QJsonDocument(QJsonDocument &&other) noexcept = default;

/*!
    Assigns \a other to this document and returns a reference to it.
*/
QJsonDocument &QJsonDocument::operator=(const QJsonDocument &other) = default;

/*!
    Moves \a other into this document and returns a reference to it.
*/
QJsonDocument &QJsonDocument::operator=(QJsonDocument &&other) noexcept = default;

/*!
    Swaps this document with \a other.
*/
void QJsonDocument::swap(QJsonDocument &other) noexcept { m_root.swap(other.m_root); }

/*!
    Converts \a variant to a document. A QVariantMap or QVariantHash gives a
    document that holds an object, and a QVariantList or QStringList one that
    holds an array; the contents are converted with QJsonValue::fromVariant().
    Any other variant gives a null document.

    \sa toVariant(), QJsonObject::fromVariantMap(), QJsonArray::fromVariantList()
*/
QJsonDocument QJsonDocument::fromVariant(const QVariant &variant) {
    QJsonDocument doc;
    switch (variant.typeId()) {
    case QMetaType::QVariantMap:
        doc.setObject(QJsonObject::fromVariantMap(variant.toMap()));
        break;
    case QMetaType::QVariantHash:
        doc.setObject(QJsonObject::fromVariantHash(variant.toHash()));
        break;
    case QMetaType::QVariantList:
        doc.setArray(QJsonArray::fromVariantList(variant.toList()));
        break;
    case QMetaType::QStringList:
        doc.setArray(QJsonArray::fromStringList(variant.toStringList()));
        break;
    default:
        break;
    }
    return doc;
}

/*!
    Converts the document to a QVariantMap if it holds an object, or to a
    QVariantList if it holds an array. A null document gives an invalid
    QVariant.

    \sa fromVariant()
*/
QVariant QJsonDocument::toVariant() const {
    if (isArray())
        return QVariant(array().toVariantList());
    if (isObject())
        return QVariant(object().toVariantMap());
    return QVariant();
}

/*!
    Parses \a json as UTF-8 encoded JSON and returns the document. If the text
    is not a valid document the result is null, and if \a error is not null
    it receives the reason and the offset at which it was found.

    \sa toJson(), QJsonParseError
*/
QJsonDocument QJsonDocument::fromJson(const QByteArray &json, QJsonParseError *error) {
    QJsonDocument doc;
    QJsonValue root = QJsonPrivate::parse(json, error);
    if (root.isArray() || root.isObject())
        doc.m_root = std::move(root);
    return doc;
}

/*!
    Converts the document to UTF-8 JSON text in the given \a format. A null
    document gives an empty byte array.

    \sa fromJson(), JsonFormat
*/
QByteArray QJsonDocument::toJson(JsonFormat format) const {
    if (isNull())
        return QByteArray();
    std::string text = QJsonPrivate::write(m_root, format == Compact);
    if (format == Indented)
        text += '\n';
    return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

/*!
    Returns \c true if the document has no content. A document made from an
    empty array or object is not empty; only a null document is.

    \sa isNull()
*/
bool QJsonDocument::isEmpty() const { return isNull(); }

/*!
    Returns \c true if the document is null: it was default constructed, or
    it came from text that could not be parsed.

    \sa isEmpty()
*/
bool QJsonDocument::isNull() const { return m_root.isNull(); }

/*!
    Returns \c true if the document holds an array.

    \sa array(), isObject()
*/
bool QJsonDocument::isArray() const { return m_root.isArray(); }

/*!
    Returns \c true if the document holds an object.

    \sa object(), isArray()
*/
bool QJsonDocument::isObject() const { return m_root.isObject(); }

/*!
    Returns the object the document holds, or an empty object if the document
    does not hold one.

    \sa isObject(), setObject()
*/
QJsonObject QJsonDocument::object() const { return m_root.toObject(); }

/*!
    Returns the array the document holds, or an empty array if the document
    does not hold one.

    \sa isArray(), setArray()
*/
QJsonArray QJsonDocument::array() const { return m_root.toArray(); }

/*!
    Makes the document hold \a object.

    \sa object(), setArray()
*/
void QJsonDocument::setObject(const QJsonObject &object) { m_root = QJsonValue(object); }

/*!
    Makes the document hold \a array.

    \sa array(), setObject()
*/
void QJsonDocument::setArray(const QJsonArray &array) { m_root = QJsonValue(array); }

/*!
    Returns the value that \a key maps to if the document holds an object, and
    an undefined value otherwise.
*/
QJsonValue QJsonDocument::operator[](const QString &key) const { return m_root[key]; }

/*!
    \overload
*/
QJsonValue QJsonDocument::operator[](QStringView key) const { return m_root[key]; }

/*!
    \overload
*/
QJsonValue QJsonDocument::operator[](QLatin1StringView key) const { return m_root[key]; }

/*!
    \fn template <typename K, typename> QJsonValue QJsonDocument::operator[](K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \overload

    Returns the element at index \a i if the document holds an array, and an
    undefined value otherwise.
*/
QJsonValue QJsonDocument::operator[](qsizetype i) const { return m_root[i]; }

bool QJsonDocument::equals(const QJsonDocument &other) const { return m_root == other.m_root; }

QDebug operator<<(QDebug dbg, const QJsonDocument &doc) {
    dbg.nospace();
    if (doc.isArray())
        dbg << "QJsonDocument(QJsonArray(" << QJsonPrivate::write(QJsonValue(doc.array()), true).c_str() << "))";
    else
        dbg << "QJsonDocument(QJsonObject(" << QJsonPrivate::write(QJsonValue(doc.object()), true).c_str() << "))";
    dbg.space();
    return dbg.maybeSpace();
}
