// MIT License - see LICENSE
#include <QtCore/qcborstreamwriter.h>

#include <QtCore/qiodevice.h>
#include <QtCore/qlogging.h>

#include <cstring>
#include <vector>

/*!
    \class QCborStreamWriter
    \brief The QCborStreamWriter class is a simple CBOR encoder operating on a one-way stream.
    \since 5.12

    The writer writes CBOR (RFC 8949) item by item to a byte array or a device: numbers, strings,
    and arrays and maps whose contents are written between startArray() and endArray(), or
    startMap() and endMap(). In a map, the items alternate between keys and values.

    \code
    QByteArray data;
    QCborStreamWriter writer(&data);
    writer.startMap(2);
    writer.append("name");
    writer.append("Nt");
    writer.append("version");
    writer.append(6);
    writer.endMap();
    \endcode

    An array or map started with a count must get exactly that many items; one started without a
    count is ended by endArray() or endMap() whatever it got. The writer does not check that what
    it is given makes sense beyond that: a tag must be followed by the item it applies to.

    \differences
    Writing to a device is not buffered: each item is written as it comes. Strings are always
    written whole, with their length, never in chunks. There is no overload for
    \c QUtf8StringView.
    \enddifferences

    \sa QCborStreamReader, QCborValue
*/

struct QCborStreamWriter::Private {
    struct Level {
        bool map = false;
        bool counted = false;
        quint64 remaining = 0; // items, counting keys and values of a map
    };

    QIODevice *device = nullptr;
    QByteArray *array = nullptr;
    std::vector<Level> levels;

    void write(const char *data, qsizetype size) {
        if (size <= 0)
            return;
        if (array)
            array->append(data, size);
        else if (device)
            device->write(data, size);
    }

    // The first byte and the argument of an item, in as few bytes as the argument needs.
    void head(quint8 major, quint64 value) {
        char bytes[9];
        qsizetype size = 1;
        const auto first = [major](unsigned info) { return static_cast<char>((unsigned{major} << 5u) | info); };
        const auto put = [&](int count) {
            for (int i = 0; i < count; ++i)
                bytes[1 + i] = static_cast<char>((value >> static_cast<unsigned>(8 * (count - 1 - i))) & 0xffu);
            size = 1 + count;
        };
        if (value < 24) {
            bytes[0] = first(static_cast<unsigned>(value));
        } else if (value <= 0xff) {
            bytes[0] = first(24);
            put(1);
        } else if (value <= 0xffff) {
            bytes[0] = first(25);
            put(2);
        } else if (value <= 0xffffffffULL) {
            bytes[0] = first(26);
            put(4);
        } else {
            bytes[0] = first(27);
            put(8);
        }
        write(bytes, size);
    }

    // Counts an item against the container it is in. A tag is part of the item that follows it.
    void counted() {
        if (levels.empty() || !levels.back().counted)
            return;
        Level &level = levels.back();
        if (level.remaining == 0)
            qWarning("QCborStreamWriter: too many items added to the array or map");
        else
            --level.remaining;
    }

    void item(quint8 major, quint64 value) {
        counted();
        head(major, value);
    }

    void bytes(quint8 major, const char *data, qsizetype size) {
        item(major, static_cast<quint64>(size));
        write(data, size);
    }

    void start(bool map, quint64 count) {
        item(map ? 5 : 4, count);
        levels.push_back(Level{map, true, map ? count * 2 : count});
    }

    void startIndefinite(bool map) {
        counted();
        const char byte = static_cast<char>(map ? 0xbf : 0x9f);
        write(&byte, 1);
        levels.push_back(Level{map, false, 0});
    }

    bool end(bool map) {
        if (levels.empty() || levels.back().map != map) {
            qWarning(map ? "QCborStreamWriter::endMap called without a map"
                         : "QCborStreamWriter::endArray called without an array");
            return false;
        }
        const Level level = levels.back();
        levels.pop_back();
        if (!level.counted) {
            const char brk = static_cast<char>(0xff);
            write(&brk, 1);
            return true;
        }
        return level.remaining == 0;
    }
};

/*!
    Constructs a writer that writes to \a device, which must be open for writing.

    \sa setDevice()
*/
QCborStreamWriter::QCborStreamWriter(QIODevice *device) : d(std::make_unique<Private>()) { d->device = device; }

/*!
    Constructs a writer that appends to \a data.
*/
QCborStreamWriter::QCborStreamWriter(QByteArray *data) : d(std::make_unique<Private>()) { d->array = data; }

/*!
    Destroys the writer. Arrays and maps that are still open are not closed.
*/
QCborStreamWriter::~QCborStreamWriter() = default;

/*!
    Makes the writer write to \a device from now on. Arrays and maps that are open stay open:
    their items go to the new device.

    \sa device()
*/
void QCborStreamWriter::setDevice(QIODevice *device) {
    d->device = device;
    d->array = nullptr;
}

/*!
    Returns the device that the writer writes to, or \c nullptr if it writes to a byte array.

    \sa setDevice()
*/
QIODevice *QCborStreamWriter::device() const { return d->device; }

/*!
    Writes the unsigned integer \a u.
*/
void QCborStreamWriter::append(quint64 u) { d->item(0, u); }

/*!
    \overload
    Writes the integer \a i, as an unsigned integer if it is not negative and as a negative
    integer otherwise.
*/
void QCborStreamWriter::append(qint64 i) {
    if (i >= 0)
        d->item(0, static_cast<quint64>(i));
    else
        d->item(1, ~static_cast<quint64>(i)); // -1 - i
}

/*!
    \overload
    Writes the negative integer \a n, which may be as small as -2^64.
*/
void QCborStreamWriter::append(QCborNegativeInteger n) { d->item(1, static_cast<quint64>(n) - 1); }

/*!
    \overload
    Writes \a ba as a byte string.

    \sa appendByteString()
*/
void QCborStreamWriter::append(const QByteArray &ba) { d->bytes(2, ba.constData(), ba.size()); }

/*!
    \overload
    Writes \a str as a text string, converted from Latin-1 to UTF-8.
*/
void QCborStreamWriter::append(QLatin1StringView str) {
    const QByteArray utf8 = QString(str).toUtf8();
    d->bytes(3, utf8.constData(), utf8.size());
}

/*!
    \overload
    Writes \a str as a text string in UTF-8.
*/
void QCborStreamWriter::append(QStringView str) {
    const QByteArray utf8 = str.toUtf8();
    d->bytes(3, utf8.constData(), utf8.size());
}

/*!
    \overload
    Writes the tag \a tag. The next item is what the tag applies to.
*/
void QCborStreamWriter::append(QCborTag tag) { d->head(6, static_cast<quint64>(tag)); }

/*!
    \overload
    Writes the tag \a tag. The next item is what the tag applies to.
*/
void QCborStreamWriter::append(QCborKnownTags tag) { append(QCborTag(static_cast<quint64>(tag))); }

/*!
    \overload
    Writes the simple type \a st.
*/
void QCborStreamWriter::append(QCborSimpleType st) { d->item(7, static_cast<quint64>(st)); }

/*!
    \overload
    Writes \c null.

    \sa appendNull()
*/
void QCborStreamWriter::append(std::nullptr_t) { appendNull(); }

/*!
    \overload
    Writes the floating-point number \a f in 16 bits.
*/
void QCborStreamWriter::append(qfloat16 f) {
    d->counted();
    const quint16 bits = QtPrivate::floatToHalfBits(static_cast<float>(f));
    const char bytes[3] = {static_cast<char>(0xf9), static_cast<char>(bits >> 8u), static_cast<char>(bits & 0xffu)};
    d->write(bytes, 3);
}

/*!
    \overload
    Writes the floating-point number \a f in 32 bits.
*/
void QCborStreamWriter::append(float f) {
    d->counted();
    quint32 bits = 0;
    std::memcpy(&bits, &f, sizeof bits);
    char bytes[5] = {static_cast<char>(0xfa)};
    for (int i = 0; i < 4; ++i)
        bytes[1 + i] = static_cast<char>((bits >> static_cast<unsigned>(8 * (3 - i))) & 0xffu);
    d->write(bytes, 5);
}

/*!
    \overload
    Writes the floating-point number \a value in 64 bits.
*/
void QCborStreamWriter::append(double value) {
    d->counted();
    quint64 bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    char bytes[9] = {static_cast<char>(0xfb)};
    for (int i = 0; i < 8; ++i)
        bytes[1 + i] = static_cast<char>((bits >> static_cast<unsigned>(8 * (7 - i))) & 0xffu);
    d->write(bytes, 9);
}

/*!
    \overload
    Writes \c true or \c false, according to \a b.
*/
void QCborStreamWriter::append(bool b) { append(b ? QCborSimpleType::True : QCborSimpleType::False); }

/*!
    \overload
    Writes the integer \a i.
*/
void QCborStreamWriter::append(int i) { append(static_cast<qint64>(i)); }

/*!
    \overload
    Writes the unsigned integer \a u.
*/
void QCborStreamWriter::append(uint u) { append(static_cast<quint64>(u)); }

/*!
    \overload
    Writes the \a size bytes at \a str as a text string; they must be UTF-8. With a negative
    \a size, \a str is taken up to its terminating zero.

    \sa appendTextString()
*/
void QCborStreamWriter::append(const char *str, qsizetype size) {
    if (size < 0)
        size = str ? static_cast<qsizetype>(std::strlen(str)) : 0;
    appendTextString(str, size);
}

/*!
    Writes the \a len bytes at \a data as a byte string.
*/
void QCborStreamWriter::appendByteString(const char *data, qsizetype len) { d->bytes(2, data, len); }

/*!
    Writes the \a len bytes at \a utf8 as a text string. They must be valid UTF-8; the writer
    does not check.
*/
void QCborStreamWriter::appendTextString(const char *utf8, qsizetype len) { d->bytes(3, utf8, len); }

/*!
    Writes \c null.
*/
void QCborStreamWriter::appendNull() { append(QCborSimpleType::Null); }

/*!
    Writes \c undefined.
*/
void QCborStreamWriter::appendUndefined() { append(QCborSimpleType::Undefined); }

/*!
    Starts an array whose length is not known yet. Write its elements, then call endArray().

    \sa startMap()
*/
void QCborStreamWriter::startArray() { d->startIndefinite(false); }

/*!
    \overload
    Starts an array of \a count elements. Write them, then call endArray().
*/
void QCborStreamWriter::startArray(quint64 count) { d->start(false, count); }

/*!
    Ends the array that was started last. Returns \c false if there is no array to end, or the
    array got another number of elements than its count.

    \sa startArray()
*/
bool QCborStreamWriter::endArray() { return d->end(false); }

/*!
    Starts a map whose length is not known yet. Write its keys and values one after the other,
    then call endMap().

    \sa startArray()
*/
void QCborStreamWriter::startMap() { d->startIndefinite(true); }

/*!
    \overload
    Starts a map of \a count pairs of a key and a value. Write them, then call endMap().
*/
void QCborStreamWriter::startMap(quint64 count) { d->start(true, count); }

/*!
    Ends the map that was started last. Returns \c false if there is no map to end, or the map
    got another number of pairs than its count.

    \sa startMap()
*/
bool QCborStreamWriter::endMap() { return d->end(true); }
