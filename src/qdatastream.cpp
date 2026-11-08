// MIT License - see LICENSE
#include <QtCore/qbuffer.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qlogging.h>

#include <algorithm>
#include <cstring>

/*!
    \class QDataStream
    \brief The QDataStream class provides serialization of binary data to a QIODevice.

    A data stream is a binary stream of encoded information that is
    independent of the host's operating system, CPU and byte order. The
    format is compatible with Qt's QDataStream: numbers are big-endian by
    default, a QString is written as UTF-16 code units behind a byte-length
    prefix, and a null QString or QByteArray is written as the length
    \c 0xFFFFFFFF.

    Write values with \c{<<} and read them back, in the same order, with
    \c{>>}:

    \code
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << qint32(42) << QString("text");

    QDataStream in(bytes);
    qint32 number;
    QString text;
    in >> number >> text;
    \endcode

    Read and write errors are recorded in status() instead of being
    reported by return values. Reading a QList, QMap or QHash clears the
    container if an error occurs. Enumerations are streamed as their
    underlying integer type.

    \differences
    Container sizes are \c quint32 only; the 64-bit extended size used by
    stream version 21 (Qt 6.7) for more than \c 0xFFFFFFFD elements is
    neither written nor read. Writing such a size sets \c SizeLimitExceeded.
Streams of version 1 (Qt 1.x Latin-1 strings) are not
    special-cased. Streaming is supported for the built-in numbers, QString,
    QChar, QByteArray, QList, QMap, QHash and enumerations only.
    \enddifferences

    \sa QTextStream, QIODevice, QBuffer, QFile
*/

namespace {

constexpr quint32 kNullSize = 0xFFFFFFFFu;
constexpr qint64 kReadBlock = 1 << 20;

bool hostIsLittleEndian() {
    const quint16 probe = 1;
    quint8 first;
    std::memcpy(&first, &probe, 1);
    return first == 1;
}

// Reads `len` bytes in blocks so a corrupt length cannot trigger a huge up-front allocation.
bool readBlocks(QDataStream &in, quint32 len, QByteArray &out) {
    out = QByteArray(static_cast<const char *>(""), 0);
    quint32 done = 0;
    while (done < len) {
        const qint64 step = std::min<qint64>(kReadBlock, len - done);
        out.resize(static_cast<qsizetype>(done + step));
        const int n = in.readRawData(out.data() + done, static_cast<int>(step));
        if (n != step) {
            out.clear();
            return false;
        }
        done += static_cast<quint32>(step);
    }
    return true;
}

} // namespace

/*!
    Constructs a data stream that operates on the byte array \a array, which
    must outlive the stream. The stream owns an internal QBuffer opened with
    the given \a mode (QIODevice::ReadOnly, QIODevice::WriteOnly or
    QIODevice::ReadWrite). Written data is stored in \a array.

    \sa QBuffer
*/
QDataStream::QDataStream(QByteArray *array, QIODeviceBase::OpenMode mode) {
    auto buf = std::make_unique<QBuffer>(array);
    buf->open(mode);
    dev = buf.get();
    owned = std::move(buf);
}

/*!
    Constructs a read-only data stream that operates on a copy of \a data.
*/
QDataStream::QDataStream(const QByteArray &data) {
    auto buf = std::make_unique<QBuffer>();
    buf->setData(data);
    buf->open(QIODeviceBase::ReadOnly);
    dev = buf.get();
    owned = std::move(buf);
}

/*!
    Destroys the data stream. Any device given to the constructor or to
    setDevice() is left open; an internal buffer created by the byte array
    constructors is released.
*/
QDataStream::~QDataStream() = default;

/*!
    Sets the I/O device to \a d, which can be \c nullptr to unset the current
    device. The stream does not take ownership of \a d. An internal buffer
    created by a byte array constructor is released.

    \sa device()
*/
void QDataStream::setDevice(QIODevice *d) {
    owned.reset();
    dev = d;
}

bool QDataStream::readSwapped(void *out, int size) {
    if (!dev)
        return false;
    unsigned char raw[8];
    if (dev->read(reinterpret_cast<char *>(raw), size) != size) {
        setStatus(ReadPastEnd);
        return false;
    }
    auto *dst = static_cast<unsigned char *>(out);
    const bool swap = (order == LittleEndian) != hostIsLittleEndian();
    for (int i = 0; i < size; ++i)
        dst[i] = swap ? raw[size - 1 - i] : raw[i];
    return true;
}

QDataStream &QDataStream::writeSwapped(const void *in, int size) {
    if (!dev || st != Ok)
        return *this;
    unsigned char raw[8];
    const auto *src = static_cast<const unsigned char *>(in);
    const bool swap = (order == LittleEndian) != hostIsLittleEndian();
    for (int i = 0; i < size; ++i)
        raw[i] = swap ? src[size - 1 - i] : src[i];
    if (dev->write(reinterpret_cast<const char *>(raw), size) != size)
        setStatus(WriteFailed);
    return *this;
}

QDataStream &QDataStream::operator>>(qint8 &v) {
    v = 0;
    readSwapped(&v, 1);
    return *this;
}

QDataStream &QDataStream::operator>>(qint16 &v) {
    v = 0;
    readSwapped(&v, 2);
    return *this;
}

QDataStream &QDataStream::operator>>(qint32 &v) {
    v = 0;
    readSwapped(&v, 4);
    return *this;
}

QDataStream &QDataStream::operator>>(qint64 &v) {
    v = 0;
    readSwapped(&v, 8);
    return *this;
}

QDataStream &QDataStream::operator>>(bool &v) {
    qint8 b = 0;
    *this >> b;
    v = b != 0;
    return *this;
}

QDataStream &QDataStream::operator>>(float &v) {
    v = 0;
    if (ver >= Qt_4_6 && precision == DoublePrecision) {
        double d = 0;
        *this >> d;
        v = static_cast<float>(d);
        return *this;
    }
    quint32 bits = 0;
    if (readSwapped(&bits, 4))
        std::memcpy(&v, &bits, sizeof v);
    return *this;
}

QDataStream &QDataStream::operator>>(double &v) {
    v = 0;
    if (ver >= Qt_4_6 && precision == SinglePrecision) {
        float f = 0;
        *this >> f;
        v = static_cast<double>(f);
        return *this;
    }
    quint64 bits = 0;
    if (readSwapped(&bits, 8))
        std::memcpy(&v, &bits, sizeof v);
    return *this;
}

QDataStream &QDataStream::operator>>(char *&s) {
    qint64 len = 0;
    return readBytes(s, len);
}

QDataStream &QDataStream::operator<<(qint8 v) { return writeSwapped(&v, 1); }
QDataStream &QDataStream::operator<<(qint16 v) { return writeSwapped(&v, 2); }
QDataStream &QDataStream::operator<<(qint32 v) { return writeSwapped(&v, 4); }
QDataStream &QDataStream::operator<<(qint64 v) { return writeSwapped(&v, 8); }

QDataStream &QDataStream::operator<<(float v) {
    if (ver >= Qt_4_6 && precision == DoublePrecision)
        return *this << static_cast<double>(v);
    quint32 bits;
    std::memcpy(&bits, &v, sizeof bits);
    return writeSwapped(&bits, 4);
}

QDataStream &QDataStream::operator<<(double v) {
    if (ver >= Qt_4_6 && precision == SinglePrecision)
        return *this << static_cast<float>(v);
    quint64 bits;
    std::memcpy(&bits, &v, sizeof bits);
    return writeSwapped(&bits, 8);
}

QDataStream &QDataStream::operator<<(const char *s) {
    if (!s)
        return *this << static_cast<quint32>(0);
    return writeBytes(s, static_cast<qint64>(std::strlen(s)) + 1);
}

/*!
    Reads the buffer \a s from the stream and returns a reference to the
    stream.

    The buffer \a s is allocated using \c{new char[]}, and null-terminated.
    Ownership passes to the caller, who must free it with \c{delete[]}. The
    length of the data, not counting the terminator, is stored in \a len. Both
    are cleared to \c nullptr and \c 0 if the length is zero or if an error
    occurs.

    The data is expected in the format written by writeBytes().

    \sa readRawData(), writeBytes()
*/
QDataStream &QDataStream::readBytes(char *&s, qint64 &len) {
    s = nullptr;
    len = 0;
    if (!dev)
        return *this;
    quint32 size = 0;
    *this >> size;
    if (size == 0 || st != Ok)
        return *this;
    QByteArray data;
    if (!readBlocks(*this, size, data))
        return *this;
    s = new char[size + 1];
    std::memcpy(s, data.constData(), size);
    s[size] = '\0';
    len = size;
    return *this;
}

/*!
    Reads at most \a len bytes from the stream into \a s and returns the
    number of bytes read, or \c -1 if there is no device or \a len is
    negative. If fewer than \a len bytes could be read, the status is set to
    \c ReadPastEnd. The bytes are read as they are, with no length prefix
    and no byte order conversion.

    \sa readBytes(), writeRawData(), skipRawData()
*/
int QDataStream::readRawData(char *s, int len) {
    if (!dev || len < 0)
        return -1;
    const qint64 n = dev->read(s, len);
    if (n != len)
        setStatus(ReadPastEnd);
    return static_cast<int>(n);
}

/*!
    Writes the length specifier \a len and the buffer \a s to the stream and
    returns a reference to the stream.

    The \a len is serialized as a \c quint32, followed by \a len bytes from
    \a s. If \a len is negative or too large for a \c quint32, nothing is
    written and the status is set to \c SizeLimitExceeded.

    \sa writeRawData(), readBytes()
*/
QDataStream &QDataStream::writeBytes(const char *s, qint64 len) {
    if (len < 0 || len >= 0xFFFFFFFELL) {
        setStatus(SizeLimitExceeded);
        return *this;
    }
    *this << static_cast<quint32>(len);
    writeRawData(s, static_cast<int>(len));
    return *this;
}

/*!
    Writes \a len bytes from \a s to the stream and returns the number of
    bytes written, or \c -1 if there is no device, the stream is in an error
    state or \a len is negative. If the device does not accept all the bytes,
    the status is set to \c WriteFailed. The bytes are written as they are,
    with no length prefix and no byte order conversion.

    \sa writeBytes(), readRawData()
*/
int QDataStream::writeRawData(const char *s, int len) {
    if (!dev || st != Ok || len < 0)
        return -1;
    const qint64 n = dev->write(s, len);
    if (n != len)
        setStatus(WriteFailed);
    return static_cast<int>(n);
}

/*!
    Skips \a len bytes from the stream and returns the number of bytes
    actually skipped, or \c -1 if there is no device or \a len is negative. If
    fewer than \a len bytes could be skipped, the status is set to
    \c ReadPastEnd.

    \sa readRawData()
*/
int QDataStream::skipRawData(int len) {
    if (!dev || len < 0)
        return -1;
    char scratch[4096];
    int skipped = 0;
    while (skipped < len) {
        const qint64 n = dev->read(scratch, std::min<qint64>(sizeof scratch, len - skipped));
        if (n <= 0)
            break;
        skipped += static_cast<int>(n);
    }
    if (skipped < len)
        setStatus(ReadPastEnd);
    return skipped;
}

/*!
    \since 5.7
    Starts a read transaction on the stream, and on its device for the
    outermost call. Transactions may be nested; only the outermost one acts on
    the device.

    Use transactions to read a message that may not have arrived completely:
    read it, then call commitTransaction(). If the data ran out, the read
    position goes back to where the transaction started, so the message can be
    read again when more data is available:

    \code
    in.startTransaction();
    QString name;
    qint32 value;
    in >> name >> value;
    if (!in.commitTransaction())
        return; // wait for more data
    \endcode

    The outermost call resets the status to \c Ok.

    \sa commitTransaction(), rollbackTransaction(), abortTransaction()
*/
void QDataStream::startTransaction() {
    if (!dev)
        return;
    if (++transactionDepth == 1) {
        dev->startTransaction();
        resetStatus();
    }
}

/*!
    \since 5.7
    Completes a read transaction. Returns \c true if no read error occurred
    during the transaction; otherwise returns \c false.

    For the outermost transaction, if the status is \c ReadPastEnd the device
    transaction is rolled back, so that the data is read again later;
    otherwise it is committed. An inner transaction only reports the status.

    \sa startTransaction(), rollbackTransaction()
*/
bool QDataStream::commitTransaction() {
    if (transactionDepth == 0) {
        qWarning("QDataStream: No transaction in progress");
        return false;
    }
    if (--transactionDepth == 0 && dev) {
        if (st == ReadPastEnd) {
            dev->rollbackTransaction();
            return false;
        }
        dev->commitTransaction();
    }
    return st == Ok;
}

/*!
    \since 5.7
    Reverts a read transaction, typically because the data read so far is
    incomplete. Sets the status to \c ReadPastEnd unless another error was
    already recorded. For the outermost transaction, the device transaction is
    rolled back if the status is \c ReadPastEnd and committed otherwise.

    \sa startTransaction(), abortTransaction()
*/
void QDataStream::rollbackTransaction() {
    setStatus(ReadPastEnd);
    if (transactionDepth == 0) {
        qWarning("QDataStream: No transaction in progress");
        return;
    }
    if (--transactionDepth != 0 || !dev)
        return;
    if (st == ReadPastEnd)
        dev->rollbackTransaction();
    else
        dev->commitTransaction();
}

/*!
    \since 5.7
    Aborts a read transaction because the data is corrupt. Sets the status to
    \c ReadCorruptData, and for the outermost transaction commits the device
    transaction, so that the bad data is not read again.

    \sa startTransaction(), rollbackTransaction()
*/
void QDataStream::abortTransaction() {
    st = ReadCorruptData;
    if (transactionDepth == 0) {
        qWarning("QDataStream: No transaction in progress");
        return;
    }
    if (--transactionDepth != 0 || !dev)
        return;
    dev->commitTransaction();
}

/*!
    \since 5.7
    Returns \c true if a read transaction is in progress on the stream's
    device; otherwise returns \c false.

    \sa QIODevice::isTransactionStarted()
*/
bool QDataStream::isDeviceTransactionStarted() const { return dev && dev->isTransactionStarted(); }

QDataStream &operator<<(QDataStream &out, const QByteArray &ba) {
    if (ba.isNull())
        return out << kNullSize;
    return out.writeBytes(ba.constData(), ba.size());
}

QDataStream &operator>>(QDataStream &in, QByteArray &ba) {
    ba.clear();
    quint32 len = 0;
    in >> len;
    if (in.status() != QDataStream::Ok || len == kNullSize)
        return in;
    readBlocks(in, len, ba);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QString &s) {
    if (s.isNull())
        return out << kNullSize;
    const qsizetype bytes = s.size() * 2;
    if (bytes >= 0xFFFFFFFELL) {
        out.setStatus(QDataStream::SizeLimitExceeded);
        return out;
    }
    QByteArray buf(bytes, '\0');
    const bool little = out.byteOrder() == QDataStream::LittleEndian;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const char16_t u = s[i].unicode();
        buf[2 * i] = static_cast<char>(little ? u & 0xFF : u >> 8);
        buf[2 * i + 1] = static_cast<char>(little ? u >> 8 : u & 0xFF);
    }
    out << static_cast<quint32>(bytes);
    out.writeRawData(buf.constData(), static_cast<int>(bytes));
    return out;
}

QDataStream &operator>>(QDataStream &in, QString &s) {
    s.clear();
    quint32 len = 0;
    in >> len;
    if (in.status() != QDataStream::Ok || len == kNullSize)
        return in;
    if (len & 1u) {
        in.setStatus(QDataStream::ReadCorruptData);
        return in;
    }
    QByteArray buf;
    if (!readBlocks(in, len, buf))
        return in;
    const bool little = in.byteOrder() == QDataStream::LittleEndian;
    QString r(qsizetype(len / 2), QChar());
    for (qsizetype i = 0; i < r.size(); ++i) {
        const auto lo = static_cast<unsigned char>(buf[2 * i + (little ? 0 : 1)]);
        const auto hi = static_cast<unsigned char>(buf[2 * i + (little ? 1 : 0)]);
        r[i] = QChar(static_cast<char16_t>((hi << 8) | lo));
    }
    s = r;
    return in;
}

QDataStream &operator<<(QDataStream &out, QChar c) { return out << static_cast<quint16>(c.unicode()); }

QDataStream &operator>>(QDataStream &in, QChar &c) {
    quint16 u = 0;
    in >> u;
    c = QChar(static_cast<char16_t>(u));
    return in;
}
