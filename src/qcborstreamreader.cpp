// MIT License - see LICENSE
#include <QtCore/qcborstreamreader.h>

#include <QtCore/qiodevice.h>

#include "qcborutf8_p.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

/*!
    \class QCborStreamReader
    \brief The QCborStreamReader class is a simple CBOR stream decoder, operating on either a QByteArray or a QIODevice.
    \since 5.12

    The reader reads CBOR (RFC 8949) item by item. type() says what the current item is, the
    \c to functions return its value, and next() moves on to the next item. An array or map is
    entered with enterContainer(); then its items are read until hasNext() is \c false, and
    leaveContainer() moves on to the item after the container. In a map the items alternate
    between keys and values. Strings are read with readString() and readByteArray(), which return
    them in chunks, or whole with readAllString() and readAllByteArray().

    \code
    QCborStreamReader reader(data);
    if (reader.isMap() && reader.enterContainer()) {
        while (reader.hasNext()) {
            const QString key = reader.readAllString();
            if (key == u"name" && reader.isString())
                name = reader.readAllString();
            else
                reader.next(); // skips the value, whatever it is
        }
        reader.leaveContainer();
    }
    if (reader.lastError() != QCborError::NoError)
        qWarning() << reader.lastError().toString();
    \endcode

    Data can arrive in pieces: when the reader runs out of data in the middle of an item, it
    reports QCborError::EndOfFile. Add the next piece with addData() and call reparse(), and the
    reader goes on from where it was.

    \differences
    The reader keeps all the data it has read from a device, so that it can go back to an item
    that was cut short. readStringChunk() cuts a chunk that does not fit at the size of the buffer. A tag counts as an item of its own, as in Qt: next()
    moves from the tag to the item that it applies to.
    \enddifferences

    \sa QCborStreamWriter, QCborValue
*/

struct QCborStreamReader::Private {
    struct Level {
        Type type = Invalid;
        qint64 remaining = -1; // items left, keys and values counted apart; -1 for a container ended by a break
    };
    enum StringState { NotStarted, InChunks, Done };

    QIODevice *device = nullptr;
    QByteArray data;
    qsizetype pos = 0; // where the current item starts
    std::vector<Level> levels;
    bool readTopLevel = false;

    Type type = Invalid;
    quint64 value = 0;
    bool lengthKnown = true;
    qsizetype headerSize = 0;
    QCborError error;

    StringState stringState = NotStarted;
    qsizetype stringPos = 0; // where the next chunk of a string starts

    // Makes sure that the bytes up to `end` are there, reading from the device if needed.
    bool need(qsizetype end) {
        while (data.size() < end) {
            if (!device)
                return false;
            const QByteArray more = device->readAll();
            if (more.isEmpty())
                return false;
            data.append(more);
        }
        return true;
    }

    quint8 byteAt(qsizetype at) const { return static_cast<quint8>(data.at(at)); }

    quint64 bigEndian(qsizetype at, int size) const {
        quint64 result = 0;
        for (int i = 0; i < size; ++i)
            result = (result << 8u) | quint64{byteAt(at + i)};
        return result;
    }

    void fail(QCborError::Code code) {
        error.c = code;
        type = Invalid;
    }

    // Reads the head of the item at pos: its type, its argument and the size of the head.
    void preparse() {
        type = Invalid;
        stringState = NotStarted;
        if (error.c != QCborError::NoError)
            return;
        if (!levels.empty() && levels.back().remaining == 0)
            return; // the end of the container
        if (!need(pos + 1)) {
            // At the top level the data may simply be over; anywhere else the container is cut short.
            if (!levels.empty() || !readTopLevel)
                error.c = QCborError::EndOfFile;
            return;
        }
        const quint8 first = byteAt(pos);
        if (first == 0xff) {
            if (levels.empty() || levels.back().remaining >= 0)
                fail(QCborError::UnexpectedBreak);
            return; // the end of a container ended by a break
        }
        const quint8 major = first >> 5;
        const quint8 info = first & 0x1f;
        lengthKnown = true;
        headerSize = 1;
        value = info;
        if (info >= 24 && info <= 27) {
            const int size = 1 << (info - 24);
            if (!need(pos + 1 + size)) {
                error.c = QCborError::EndOfFile;
                return;
            }
            value = bigEndian(pos + 1, size);
            headerSize = 1 + size;
        } else if (info == 31) {
            if (major < 2 || major > 5) {
                fail(major == 7 ? QCborError::UnexpectedBreak : QCborError::IllegalNumber);
                return;
            }
            lengthKnown = false;
            value = 0;
        } else if (info > 27) {
            fail(major == 7 ? QCborError::UnknownType : QCborError::IllegalNumber);
            return;
        }

        switch (major) {
        case 0:
            type = UnsignedInteger;
            break;
        case 1:
            type = NegativeInteger;
            break;
        case 2:
        case 3:
            if (lengthKnown && value > static_cast<quint64>(std::numeric_limits<qsizetype>::max() / 2)) {
                fail(QCborError::DataTooLarge);
                return;
            }
            type = major == 2 ? ByteString : TextString;
            break;
        case 4:
        case 5:
            if (lengthKnown && value > static_cast<quint64>(std::numeric_limits<qint64>::max() / 2)) {
                fail(QCborError::DataTooLarge);
                return;
            }
            type = major == 4 ? Array : Map;
            break;
        case 6:
            type = Tag;
            break;
        default:
            if (info == 25) {
                type = HalfFloat;
            } else if (info == 26) {
                type = Float;
            } else if (info == 27) {
                type = Double;
            } else if (info == 24 && value < 32) {
                fail(QCborError::IllegalSimpleType);
            } else {
                type = SimpleType;
            }
            break;
        }
    }

    // The item that started at pos ends at `end`: the container that holds it has one item less.
    void advance(qsizetype end, bool countsAsItem) {
        pos = end;
        if (countsAsItem) {
            if (levels.empty())
                readTopLevel = true;
            else if (levels.back().remaining > 0)
                --levels.back().remaining;
        }
        preparse();
    }

    // Reads the next chunk of the current string.
    StringResult<QByteArray> readChunk() {
        StringResult<QByteArray> result;
        if (type != ByteString && type != TextString)
            return result;
        const quint8 major = type == ByteString ? 2 : 3;
        if (stringState == NotStarted) {
            if (lengthKnown) {
                const qsizetype end = pos + headerSize + static_cast<qsizetype>(value);
                if (!need(end)) {
                    error.c = QCborError::EndOfFile;
                    return result;
                }
                result.data = data.mid(pos + headerSize, static_cast<qsizetype>(value));
                stringState = Done;
                stringPos = end;
                result.status = Ok;
                return result;
            }
            stringState = InChunks;
            stringPos = pos + headerSize;
        }
        if (stringState == Done) {
            advance(stringPos, true);
            result.status = EndOfString;
            return result;
        }
        // A string of unknown length: definite chunks of the same type, ended by a break.
        if (!need(stringPos + 1)) {
            error.c = QCborError::EndOfFile;
            return result;
        }
        const quint8 first = byteAt(stringPos);
        if (first == 0xff) {
            advance(stringPos + 1, true);
            result.status = EndOfString;
            return result;
        }
        const quint8 info = first & 0x1f;
        if ((first >> 5) != major || info == 31 || info > 27) {
            fail(QCborError::IllegalType);
            return result;
        }
        quint64 length = info;
        qsizetype head = 1;
        if (info >= 24) {
            const int size = 1 << (info - 24);
            if (!need(stringPos + 1 + size)) {
                error.c = QCborError::EndOfFile;
                return result;
            }
            length = bigEndian(stringPos + 1, size);
            head = 1 + size;
        }
        if (length > static_cast<quint64>(std::numeric_limits<qsizetype>::max() / 2)) {
            fail(QCborError::DataTooLarge);
            return result;
        }
        const qsizetype end = stringPos + head + static_cast<qsizetype>(length);
        if (!need(end)) {
            error.c = QCborError::EndOfFile;
            return result;
        }
        result.data = data.mid(stringPos + head, static_cast<qsizetype>(length));
        stringPos = end;
        result.status = Ok;
        return result;
    }

    // The size of the chunk that readChunk() would return next, or -1 at the end of the string or without the data.
    qsizetype peekChunkSize() const {
        if (type != ByteString && type != TextString)
            return -1;
        if (stringState == NotStarted && lengthKnown)
            return static_cast<qsizetype>(value);
        if (stringState == Done)
            return -1;
        const qsizetype at = stringState == NotStarted ? pos + headerSize : stringPos;
        if (at >= data.size())
            return -1;
        const quint8 first = byteAt(at);
        const quint8 info = first & 0x1f;
        if (first == 0xff || info == 31 || info > 27)
            return -1;
        if (info < 24)
            return info;
        const int size = 1 << (info - 24);
        if (at + 1 + size > data.size())
            return -1;
        const quint64 length = bigEndian(at + 1, size);
        return length > static_cast<quint64>(std::numeric_limits<qsizetype>::max() / 2)
                   ? -1
                   : static_cast<qsizetype>(length);
    }

    // A text string must be valid UTF-8 in each chunk.
    StringResult<QByteArray> readTextChunk() {
        StringResult<QByteArray> result = readChunk();
        if (result.status == Ok && type == TextString && !QtPrivate::isValidUtf8(result.data)) {
            fail(QCborError::InvalidUtf8String);
            result.data.clear();
            result.status = Error;
        }
        return result;
    }
};

/*!
    Constructs a reader without data. Give it data with addData() or a device with setDevice().
*/
QCborStreamReader::QCborStreamReader() : d(std::make_unique<Private>()) { d->preparse(); }

/*!
    Constructs a reader that reads the \a len bytes at \a data, which it copies.
*/
QCborStreamReader::QCborStreamReader(const char *data, qsizetype len) : QCborStreamReader(QByteArray(data, len)) {}

/*!
    \overload
    Constructs a reader that reads the \a len bytes at \a data, which it copies.
*/
QCborStreamReader::QCborStreamReader(const quint8 *data, qsizetype len)
    : QCborStreamReader(QByteArray(reinterpret_cast<const char *>(data), len)) {}

/*!
    Constructs a reader that reads \a data.
*/
QCborStreamReader::QCborStreamReader(const QByteArray &data) : d(std::make_unique<Private>()) {
    d->data = data;
    d->preparse();
}

/*!
    Constructs a reader that reads from \a device, which must be open for reading.
*/
QCborStreamReader::QCborStreamReader(QIODevice *device) : d(std::make_unique<Private>()) {
    d->device = device;
    d->preparse();
}

/*!
    Destroys the reader. The device is not closed.
*/
QCborStreamReader::~QCborStreamReader() = default;

/*!
    Makes the reader read from \a device, from its current position, and forget the data it was
    reading.

    \sa device(), clear()
*/
void QCborStreamReader::setDevice(QIODevice *device) {
    clear();
    d->device = device;
    d->error.c = QCborError::NoError;
    d->preparse();
}

/*!
    Returns the device that the reader reads from, or \c nullptr if it reads data that was
    given to it.

    \sa setDevice()
*/
QIODevice *QCborStreamReader::device() const { return d->device; }

/*!
    Adds \a data to the end of the data that the reader reads. Call reparse() afterwards if the
    reader stopped at the end of the data before.

    \sa reparse()
*/
void QCborStreamReader::addData(const QByteArray &data) { d->data.append(data); }

/*!
    \overload
    Adds the \a len bytes at \a data.
*/
void QCborStreamReader::addData(const char *data, qsizetype len) { d->data.append(data, len); }

/*!
    \overload
    Adds the \a len bytes at \a data.
*/
void QCborStreamReader::addData(const quint8 *data, qsizetype len) {
    d->data.append(reinterpret_cast<const char *>(data), len);
}

/*!
    Reads the current item again, after more data has been added. Call it when lastError() is
    QCborError::EndOfFile, or the reader has reached the end of the data at the top level, and
    addData() has given the reader more.

    \sa addData()
*/
void QCborStreamReader::reparse() {
    if (d->error.c == QCborError::EndOfFile)
        d->error.c = QCborError::NoError;
    else if (d->error.c != QCborError::NoError)
        return;
    // A string that was being read goes on from its next chunk; anything else is read again from its head.
    if (d->stringState == Private::NotStarted)
        d->preparse();
}

/*!
    Forgets the data, the device and the place, and puts the reader back into the state of a new
    one.

    \sa reset()
*/
void QCborStreamReader::clear() {
    d = std::make_unique<Private>();
    d->preparse();
}

/*!
    Goes back to the start of the data, to read it again.

    \sa clear()
*/
void QCborStreamReader::reset() {
    d->pos = 0;
    d->levels.clear();
    d->readTopLevel = false;
    d->error.c = QCborError::NoError;
    d->preparse();
}

/*!
    Returns the error that stopped the reader, or QCborError::NoError.
*/
QCborError QCborStreamReader::lastError() const { return d->error; }

/*!
    Returns the offset in bytes at which the current item starts.
*/
qint64 QCborStreamReader::currentOffset() const { return d->pos; }

/*!
    Returns \c true if the reader is at an item: if type() is not \c Invalid.

    \sa hasNext()
*/
bool QCborStreamReader::isValid() const { return d->type != Invalid; }

/*!
    Returns the number of containers that the reader has entered and not left.

    \sa enterContainer()
*/
int QCborStreamReader::containerDepth() const { return static_cast<int>(d->levels.size()); }

/*!
    Returns the type of the container that the reader is in, or \c Invalid at the top level.
*/
QCborStreamReader::Type QCborStreamReader::parentContainerType() const {
    return d->levels.empty() ? Invalid : d->levels.back().type;
}

/*!
    Returns \c true if there is an item to read: in a container, until its end; at the top level,
    until the end of the data.

    \sa next(), leaveContainer()
*/
bool QCborStreamReader::hasNext() const noexcept { return d->type != Invalid; }

/*!
    Moves to the next item, skipping the current one with everything in it: the elements of an
    array or map, and the chunks of a string. Containers may be nested \a maxRecursion levels
    deep. Returns \c false if there is no item to skip or the data is not valid.

    \sa enterContainer()
*/
bool QCborStreamReader::next(int maxRecursion) {
    if (d->type == Invalid) {
        if (d->error.c == QCborError::NoError && !d->levels.empty())
            d->error.c = QCborError::AdvancePastEnd;
        return false;
    }
    if (isContainer()) {
        if (maxRecursion <= 0) {
            d->fail(QCborError::NestingTooDeep);
            return false;
        }
        enterContainer();
        while (hasNext()) {
            if (!next(maxRecursion - 1))
                return false;
        }
        return d->error.c == QCborError::NoError && leaveContainer();
    }
    if (isString() || isByteArray()) {
        for (;;) {
            const StringResult<QByteArray> chunk = d->readChunk();
            if (chunk.status == EndOfString)
                return true;
            if (chunk.status == Error)
                return false;
        }
    }
    // A tag is not an item of its own in the container: the item it applies to is.
    d->advance(d->pos + d->headerSize, d->type != Tag);
    return true;
}

/*!
    Returns the type of the current item.
*/
QCborStreamReader::Type QCborStreamReader::type() const { return d->type; }

/*!
    Returns \c true if the length of the current string, array or map is known before it is
    read. Strings, arrays and maps of unknown length end with a break.

    \sa length()
*/
bool QCborStreamReader::isLengthKnown() const noexcept { return d->lengthKnown; }

/*!
    Returns the length of the current string in bytes, of the current array in elements or of
    the current map in pairs. It is 0 if the length is not known.

    \sa isLengthKnown()
*/
quint64 QCborStreamReader::length() const { return d->lengthKnown ? d->value : 0; }

/*!
    Enters the current array or map, so that its items can be read. Returns \c false if the
    current item is not a container.

    \sa leaveContainer(), next()
*/
bool QCborStreamReader::enterContainer() {
    if (!isContainer())
        return false;
    Private::Level level;
    level.type = d->type;
    level.remaining = d->lengthKnown ? static_cast<qint64>(d->type == Map ? d->value * 2 : d->value) : -1;
    d->levels.push_back(level);
    d->pos += d->headerSize;
    d->preparse();
    return true;
}

/*!
    Leaves the container after its last item has been read, and moves to the item after it.
    Returns \c false if the reader is in no container, has not read all its items, or the data
    is not valid.

    \sa enterContainer(), hasNext()
*/
bool QCborStreamReader::leaveContainer() {
    if (d->levels.empty() || d->type != Invalid || d->error.c != QCborError::NoError)
        return false;
    const bool broken = d->levels.back().remaining < 0;
    d->levels.pop_back();
    d->advance(d->pos + (broken ? 1 : 0), true);
    return true;
}

/*!
    Reads the next chunk of the current text string. Call it until the status is \c EndOfString,
    after which the reader is at the next item; a string of known length comes in one chunk.

    \sa readAllString(), readByteArray()
*/
QCborStreamReader::StringResult<QString> QCborStreamReader::readString() {
    const StringResult<QByteArray> chunk = readUtf8String();
    StringResult<QString> result;
    result.status = chunk.status;
    if (chunk.status == Ok)
        result.data = QString::fromUtf8(chunk.data);
    return result;
}

/*!
    Reads the next chunk of the current text string, in UTF-8.

    \sa readString()
*/
QCborStreamReader::StringResult<QByteArray> QCborStreamReader::readUtf8String() {
    if (!isString())
        return {};
    return d->readTextChunk();
}

/*!
    Reads the next chunk of the current byte string. Call it until the status is
    \c EndOfString, after which the reader is at the next item.

    \sa readAllByteArray()
*/
QCborStreamReader::StringResult<QByteArray> QCborStreamReader::readByteArray() {
    if (!isByteArray())
        return {};
    return d->readChunk();
}

/*!
    Returns the size in bytes of the chunk of the current string that the next read returns, or -1 if the
    current item is not a string, the string has no more chunks, or the data of the chunk has not arrived yet.

    \sa readStringChunk()
*/
qsizetype QCborStreamReader::currentStringChunkSize() const { return d->peekChunkSize(); }

/*!
    Reads the next chunk of the current string, text or bytes, into the buffer \a ptr of \a maxlen bytes, and
    returns its size in the data of the result. A chunk that does not fit is cut at \a maxlen bytes; check its size
    with currentStringChunkSize() first. If \a ptr is null, the chunk is skipped. At the end of the string, the
    status is \c EndOfString and the reader is at the next item.

    \code
    std::vector<char> buffer;
    for (;;) {
        const qsizetype size = reader.currentStringChunkSize();
        if (size >= 0)
            buffer.resize(buffer.size() + size);
        const auto r = reader.readStringChunk(buffer.data() + buffer.size() - std::max<qsizetype>(size, 0), size);
        if (r.status != QCborStreamReader::Ok)
            break;
    }
    \endcode

    \sa currentStringChunkSize(), readString()
*/
QCborStreamReader::StringResult<qsizetype> QCborStreamReader::readStringChunk(char *ptr, qsizetype maxlen) {
    StringResult<qsizetype> result;
    if (!isString() && !isByteArray())
        return result;
    const StringResult<QByteArray> chunk = isString() ? d->readTextChunk() : d->readChunk();
    result.status = chunk.status;
    if (chunk.status == Ok) {
        const qsizetype n = std::min(chunk.data.size(), std::max<qsizetype>(maxlen, 0));
        if (ptr && n > 0)
            std::memcpy(ptr, chunk.data.constData(), static_cast<std::size_t>(n));
        result.data = ptr ? n : chunk.data.size();
    }
    return result;
}

/*!
    \since 6.7
    Reads the whole current text string, appends it to \a dst and moves to the next item. Returns \c true on
    success; on an error, returns \c false, and \a dst may hold part of the string.

    \sa readAllString()
*/
bool QCborStreamReader::readAndAppendToString(QString &dst) {
    QByteArray utf8;
    if (!readAndAppendToUtf8String(utf8))
        return false;
    dst.append(QString::fromUtf8(utf8));
    return true;
}

/*!
    \since 6.7
    Reads the whole current text string in UTF-8, appends it to \a dst and moves to the next item. Returns
    \c true on success.

    \sa readAllUtf8String()
*/
bool QCborStreamReader::readAndAppendToUtf8String(QByteArray &dst) {
    if (!isString())
        return false;
    for (;;) {
        const StringResult<QByteArray> chunk = d->readTextChunk();
        if (chunk.status == EndOfString)
            return true;
        if (chunk.status == Error)
            return false;
        dst.append(chunk.data);
    }
}

/*!
    \since 6.7
    Reads the whole current byte string, appends it to \a dst and moves to the next item. Returns \c true on
    success.

    \sa readAllByteArray()
*/
bool QCborStreamReader::readAndAppendToByteArray(QByteArray &dst) {
    if (!isByteArray())
        return false;
    for (;;) {
        const StringResult<QByteArray> chunk = d->readChunk();
        if (chunk.status == EndOfString)
            return true;
        if (chunk.status == Error)
            return false;
        dst.append(chunk.data);
    }
}

/*!
    \since 6.7

    Reads the whole current text string and moves to the next item. Returns a null string if the
    current item is not a text string or there is an error.

    \sa readString()
*/
QString QCborStreamReader::readAllString() {
    const QByteArray utf8 = readAllUtf8String();
    return utf8.isNull() ? QString() : QString::fromUtf8(utf8);
}

/*!
    \since 6.7

    Reads the whole current text string in UTF-8 and moves to the next item. Returns a null byte
    array if the current item is not a text string or there is an error.
*/
QByteArray QCborStreamReader::readAllUtf8String() {
    if (!isString())
        return QByteArray();
    QByteArray all("");
    for (;;) {
        const StringResult<QByteArray> chunk = d->readTextChunk();
        if (chunk.status == EndOfString)
            return all;
        if (chunk.status == Error)
            return QByteArray();
        all.append(chunk.data);
    }
}

/*!
    \since 6.7

    Reads the whole current byte string and moves to the next item. Returns a null byte array if
    the current item is not a byte string or there is an error.
*/
QByteArray QCborStreamReader::readAllByteArray() {
    if (!isByteArray())
        return QByteArray();
    QByteArray all("");
    for (;;) {
        const StringResult<QByteArray> chunk = d->readChunk();
        if (chunk.status == EndOfString)
            return all;
        if (chunk.status == Error)
            return QByteArray();
        all.append(chunk.data);
    }
}

/*!
    Returns the number of the current tag.
*/
QCborTag QCborStreamReader::toTag() const { return isTag() ? QCborTag(d->value) : QCborTag(0); }

/*!
    Returns the value of the current unsigned integer.
*/
quint64 QCborStreamReader::toUnsignedInteger() const { return isUnsignedInteger() ? d->value : 0; }

/*!
    Returns the absolute value of the current negative integer: 1 for -1.
*/
QCborNegativeInteger QCborStreamReader::toNegativeInteger() const {
    return isNegativeInteger() ? QCborNegativeInteger(d->value + 1) : QCborNegativeInteger(0);
}

/*!
    Returns the current simple type, among them \c false, \c true, \c null and \c undefined.
*/
QCborSimpleType QCborStreamReader::toSimpleType() const {
    return d->type == SimpleType ? static_cast<QCborSimpleType>(d->value) : QCborSimpleType::Undefined;
}

/*!
    Returns the current floating-point number of 16 bits.
*/
qfloat16 QCborStreamReader::toFloat16() const {
    return isFloat16() ? qfloat16(QtPrivate::halfBitsToFloat(static_cast<quint16>(d->value))) : qfloat16(0.0f);
}

/*!
    Returns the current floating-point number of 32 bits.
*/
float QCborStreamReader::toFloat() const {
    if (!isFloat())
        return 0;
    const auto bits = static_cast<quint32>(d->value);
    float f = 0;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

/*!
    Returns the current floating-point number of 64 bits.
*/
double QCborStreamReader::toDouble() const {
    if (!isDouble())
        return 0;
    double v = 0;
    std::memcpy(&v, &d->value, sizeof v);
    return v;
}

/*!
    Returns the current integer, unsigned or negative, as a \c qint64. A number that does not
    fit wraps around.
*/
qint64 QCborStreamReader::toInteger() const {
    if (isUnsignedInteger())
        return static_cast<qint64>(d->value);
    if (isNegativeInteger())
        return static_cast<qint64>(~d->value); // -1 - value
    return 0;
}
