// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>

/*
    \enum QCborSimpleType
    \since 5.12

    The simple types of CBOR (RFC 8949): the values of major type 7 that are not floating-point
    numbers. Four of them have meanings; the others are free for applications to use.

    \value False The value \c false.
    \value True The value \c true.
    \value Null The value \c null.
    \value Undefined An undefined value: something that has no value at all.

    \sa QCborStreamReader::toSimpleType(), QCborValue::toSimpleType()
*/
enum class QCborSimpleType : quint8 { False = 20, True = 21, Null = 22, Undefined = 23 };

/*
    \enum QCborTag
    \since 5.12

    A CBOR tag: a number that says how the value that follows is to be understood. It is an
    enumeration without enumerators so that any number can be a tag; QCborKnownTags names the
    ones that the standards define.

    \sa QCborKnownTags, QCborValue::tag()
*/
enum class QCborTag : quint64 {};

/*
    \enum QCborKnownTags
    \since 5.12

    The tags of CBOR that the standards define and that Nt knows.

    \value DateTimeString A date and time as an ISO 8601 text string. QCborValue reads it as a QDateTime.
    \value UnixTime_t A date and time as the number of seconds since 1970-01-01T00:00Z.
    \value PositiveBignum A positive integer of any size, as a byte string in network byte order.
    \value NegativeBignum A negative integer of any size: -1 minus the byte string read as a positive integer.
    \value Decimal A decimal fraction: an array of an exponent of ten and a mantissa.
    \value Bigfloat A binary fraction: an array of an exponent of two and a mantissa.
    \value COSE_Encrypt0 A COSE single-recipient encrypted message.
    \value COSE_Mac0 A COSE message authenticated without recipients.
    \value COSE_Sign1 A COSE message with a single signature.
    \value ExpectedBase64url A byte string that is expected to become Base64url when converted to JSON.
    \value ExpectedBase64 A byte string that is expected to become Base64 when converted to JSON.
    \value ExpectedBase16 A byte string that is expected to become hexadecimal when converted to JSON.
    \value EncodedCbor A byte string that holds CBOR.
    \value Url A URL as a text string. QCborValue reads it as a QUrl.
    \value Base64url A text string in Base64url.
    \value Base64 A text string in Base64.
    \value RegularExpression A regular expression as a text string. QCborValue reads it as a QRegularExpression.
    \value MimeMessage A MIME message as a text string.
    \value Uuid A UUID as a byte string of 16 bytes. QCborValue reads it as a QUuid.
    \value COSE_Encrypt A COSE encrypted message.
    \value COSE_Mac A COSE authenticated message.
    \value COSE_Sign A COSE signed message.
    \value Signature The "self-describing CBOR" marker: a tag that a file may start with so that it can be recognized.
*/
enum class QCborKnownTags {
    DateTimeString = 0,
    UnixTime_t = 1,
    PositiveBignum = 2,
    NegativeBignum = 3,
    Decimal = 4,
    Bigfloat = 5,
    COSE_Encrypt0 = 16,
    COSE_Mac0 = 17,
    COSE_Sign1 = 18,
    ExpectedBase64url = 21,
    ExpectedBase64 = 22,
    ExpectedBase16 = 23,
    EncodedCbor = 24,
    Url = 32,
    Base64url = 33,
    Base64 = 34,
    RegularExpression = 35,
    MimeMessage = 36,
    Uuid = 37,
    COSE_Encrypt = 96,
    COSE_Mac = 97,
    COSE_Sign = 98,
    Signature = 55799,
};

/*
    \enum QCborNegativeInteger
    \since 5.12

    A negative integer of CBOR that may be too large for \c qint64. The value is the absolute
    value of the number: \c{QCborNegativeInteger(1)} is -1, and \c{QCborNegativeInteger(0)} stands
    for -2^64, which no unsigned 64-bit number can hold.

    \sa QCborStreamWriter::append(), QCborStreamReader::toNegativeInteger()
*/
enum class QCborNegativeInteger : quint64 {};

/*!
    \class QCborError
    \brief The QCborError class holds the error condition found while parsing or validating a CBOR stream.
    \since 5.12

    A QCborError converts to its Code, so it can be compared with an enumerator:

    \code
    if (reader.lastError() != QCborError::NoError)
        qWarning() << reader.lastError().toString();
    \endcode

    \differences
    None.
    \enddifferences

    \sa QCborStreamReader::lastError(), QCborParserError
*/
class QCborError {
public:
    /*!
        \enum QCborError::Code

        The errors of CBOR.

        \value NoError There was no error.
        \value UnknownError Something went wrong that has no code of its own.
        \value AdvancePastEnd The reader was asked to read past the end of a container.
        \value InputOutputError The device could not be read.
        \value GarbageAtEnd There is data after the end of the CBOR stream.
        \value EndOfFile The data ends before the element does; more data may come.
        \value UnexpectedBreak A break byte that ends no container of unknown length.
        \value UnknownType A type of CBOR that is not defined.
        \value IllegalType An item where its type is not allowed, such as a chunk of another type in a string.
        \value IllegalNumber A number that is not encoded properly.
        \value IllegalSimpleType A simple type below 32 that is encoded in two bytes.
        \value InvalidUtf8String A text string that is not valid UTF-8.
        \value DataTooLarge A string or container larger than this program can hold.
        \value NestingTooDeep Containers nested more deeply than the limit allows.
        \value UnsupportedType An item that the operation does not support.
    */
    enum Code : int {
        UnknownError = 1,
        AdvancePastEnd = 3,
        InputOutputError = 4,
        GarbageAtEnd = 256,
        EndOfFile,
        UnexpectedBreak,
        UnknownType,
        IllegalType,
        IllegalNumber,
        IllegalSimpleType,
        InvalidUtf8String = 516,
        DataTooLarge = 1024,
        NestingTooDeep,
        UnsupportedType,
        NoError = 0,
    };

    /*!
        Returns the code of the error.
    */
    constexpr Code code() const noexcept { return c; }
    /*!
        Returns the code of the error, so that a QCborError can be compared with a Code.
    */
    constexpr operator Code() const noexcept { return c; }
    QString toString() const;

    /*!
        The code of the error.
    */
    Code c = NoError;
};
