// MIT License - see LICENSE
#include <QtCore/qcborcommon.h>

/*!
    Returns a text that describes the error, in English.
*/
QString QCborError::toString() const {
    switch (c) {
    case NoError:
        return QString("No error");
    case UnknownError:
        return QString("Unknown error");
    case AdvancePastEnd:
        return QString("Read past end of buffer (more bytes needed)");
    case InputOutputError:
        return QString("Input/Output error");
    case GarbageAtEnd:
        return QString("Data found after the end of the stream");
    case EndOfFile:
        return QString("Unexpected end of input data (more bytes needed)");
    case UnexpectedBreak:
        return QString("Invalid CBOR stream: unexpected 'break' byte");
    case UnknownType:
        return QString("Invalid CBOR stream: unknown type");
    case IllegalType:
        return QString("Invalid CBOR stream: illegal type found");
    case IllegalNumber:
        return QString("Invalid CBOR stream: illegal number encoding (future extension)");
    case IllegalSimpleType:
        return QString("Invalid CBOR stream: illegal simple type");
    case InvalidUtf8String:
        return QString("Invalid CBOR stream: invalid UTF-8 text string");
    case DataTooLarge:
        return QString("Internal limitation: data set too large");
    case NestingTooDeep:
        return QString("JSON/CBOR conversion: nesting level too deep");
    case UnsupportedType:
        return QString("JSON/CBOR conversion: unsupported type");
    }
    return QString("Unknown error");
}
