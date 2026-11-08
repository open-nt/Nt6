// MIT License - see LICENSE
#include <QtCore/qiodevice.h>
#include <QtCore/qlogging.h>

#include <algorithm>
#include <limits>
#include <utility>

/*!
    \class QIODevice
    \brief The QIODevice class is the abstract base class of all I/O devices.

    QIODevice provides the common interface for devices that can read and
    write blocks of bytes, such as QFile, QBuffer and QTemporaryFile. A
    subclass implements readData() and writeData(), and usually reimplements
    open(), close(), pos(), size() and seek().

    Reading and writing go through read(), readLine(), readAll(), write() and
    friends. These check that the device is open in the right mode, then call
    readData() or writeData(). For sequential devices (see isSequential()),
    peek() and ungetChar() keep look-ahead bytes in an internal buffer, which
    read() and readLine() consume first.

    The device is not thread-safe, and the signals are emitted only by
    subclasses that choose to emit them.

    \differences
    There is no read-channel API (\c setCurrentReadChannel(), \c
    readChannelCount(), \c writeChannelCount(), \c channelReadyRead() and \c
    channelBytesWritten()). Only sequential devices buffer data, and the base
    class does not translate line endings in text mode. Errors are reported
    only as a string through errorString(); there is no error enum. A
    transaction on a random-access device is rolled back by seeking, and on a
    sequential device it ends when the device is opened, closed or seeks.
    \enddifferences

    \sa QFile, QBuffer, QTextStream, QDataStream
*/

/*!
    Constructs a device with no parent. The device is not open.
*/
QIODevice::QIODevice() = default;

/*!
    Constructs a device with the given \a parent. The device is not open.
*/
QIODevice::QIODevice(QObject *parent) : QObject(parent) {}

/*!
    Destroys the device. The device is not closed automatically; subclasses
    close it in their own destructors.
*/
QIODevice::~QIODevice() = default;

/*!
    If \a enabled is \c true, sets the \c Text flag on the device's open mode;
    otherwise clears it.

    \sa isTextModeEnabled(), openMode()
*/
void QIODevice::setTextModeEnabled(bool enabled) {
    if (enabled)
        m_mode |= Text;
    else
        m_mode &= ~OpenMode(Text);
}

/*!
    Opens the device and sets its open mode to \a mode. Returns \c true if
    successful; otherwise returns \c false.

    The base implementation stores \a mode, discards any buffered data and
    clears the error string, and always succeeds. Subclasses reimplement this
    function, call it once the device is open and return \c true.

    \sa close(), openMode(), isOpen()
*/
bool QIODevice::open(OpenMode mode) {
    m_mode = mode;
    m_buf.clear();
    m_bufPos = 0;
    m_transaction = false;
    m_error.clear();
    return true;
}

/*!
    First emits aboutToClose(), then closes the device and sets its open mode
    to \c NotOpen. Any buffered look-ahead data is discarded. Does nothing if
    the device is not open.

    Subclasses reimplement this function to release their resources and call
    the base implementation.

    \sa open(), aboutToClose()
*/
void QIODevice::close() {
    if (!isOpen())
        return;
    aboutToClose();
    m_mode = NotOpen;
    m_buf.clear();
    m_bufPos = 0;
    m_transaction = false;
}

/*!
    Seeks to position \a pos in the device. Returns \c true on success;
    otherwise returns \c false.

    The base implementation only discards the look-ahead buffer and always
    returns \c true. Subclasses that support random access reimplement this
    function and call it first.

    \sa pos(), reset(), isSequential()
*/
bool QIODevice::seek(qint64) {
    m_buf.clear();
    m_bufPos = 0;
    if (isSequential())
        m_transaction = false; // the kept bytes are gone
    return true;
}

/*!
    Returns \c true if the device is open and there is no more data to read;
    otherwise returns \c false.

    \sa bytesAvailable(), read()
*/
bool QIODevice::atEnd() const { return isOpen() && bytesAvailable() <= 0; }

/*!
    Returns the number of bytes that are available for reading.

    The base implementation returns the number of look-ahead bytes buffered
    by peek() or ungetChar(). Subclasses reimplement this function and add
    the base implementation's result to their own.

    \sa bytesToWrite(), read(), atEnd()
*/
qint64 QIODevice::bytesAvailable() const { return bufferedBytes(); }

/*!
    Returns \c true if a complete line of data can be read from the device;
    otherwise returns \c false.

    The base implementation only looks at the look-ahead buffer.

    \sa readLine()
*/
bool QIODevice::canReadLine() const {
    return m_buf.indexOf('\n', m_bufPos) != -1;
}

/*!
    Returns a human-readable description of the last device error, or
    \c{"Unknown error"} if no error string has been set.

    \sa setErrorString()
*/
QString QIODevice::errorString() const { return m_error.isEmpty() ? QString::fromUtf8("Unknown error") : m_error; }

void QIODevice::compactBuffer() {
    if (m_transaction)
        return; // a rollback may still need the consumed bytes
    if (m_bufPos >= m_buf.size()) {
        m_buf.clear();
        m_bufPos = 0;
    }
}

/*!
    Reads at most \a maxSize bytes from the device into \a data, and returns
    the number of bytes read. Returns \c -1 if the device is not open for
    reading or if an error occurs before any byte is read, and \c 0 if
    \a maxSize is not positive or no data could be read.

    Look-ahead data buffered by peek() or ungetChar() is returned first.

    \sa readData(), readLine(), write()
*/
qint64 QIODevice::read(char *data, qint64 maxSize) {
    if (!isReadable()) {
        setErrorString(QString::fromUtf8("device not open for reading"));
        return -1;
    }
    if (maxSize <= 0)
        return 0;
    qint64 total = 0;
    const qint64 fromBuf = std::min(bufferedBytes(), maxSize);
    if (fromBuf > 0) {
        std::copy(m_buf.constData() + m_bufPos, m_buf.constData() + m_bufPos + fromBuf, data);
        m_bufPos += fromBuf;
        total = fromBuf;
        compactBuffer();
    }
    if (total < maxSize) {
        const qint64 n = readData(data + total, maxSize - total);
        if (n > 0) {
            keepForTransaction(data + total, n);
            total += n;
        } else if (n < 0 && total == 0) {
            return -1;
        }
    }
    return total;
}

/*!
    \overload

    Reads at most \a maxSize bytes from the device and returns them as a
    QByteArray. Returns an empty byte array if \a maxSize is not positive or
    if nothing could be read.
*/
QByteArray QIODevice::read(qint64 maxSize) {
    if (maxSize <= 0)
        return QByteArray(static_cast<const char *>(""), 0);
    QByteArray r(maxSize, '\0');
    const qint64 n = read(r.data(), maxSize);
    if (n <= 0)
        return QByteArray(static_cast<const char *>(""), 0);
    r.resize(n);
    return r;
}

/*!
    Reads all remaining data from the device and returns it as a byte array.

    The data is read in chunks until read() returns no more data, so this
    function does not wait for more data on a sequential device.

    \sa read(), readLine()
*/
QByteArray QIODevice::readAll() {
    QByteArray r(static_cast<const char *>(""), 0);
    char buf[8192];
    for (;;) {
        const qint64 n = read(buf, sizeof buf);
        if (n <= 0)
            break;
        r.append(buf, n);
    }
    return r;
}

/*!
    Reads up to \a maxSize bytes from the device into \a data, stopping after
    the first newline, and returns the number of bytes read.

    The base implementation calls readData() for one byte at a time.
    Subclasses reimplement it when they can find the line end more
    efficiently. Look-ahead data is handled by readLine() before this
    function is called.

    \sa readLine(), readData()
*/
qint64 QIODevice::readLineData(char *data, qint64 maxSize) {
    qint64 n = 0;
    while (n < maxSize) {
        char c;
        const qint64 r = readData(&c, 1);
        if (r <= 0)
            break;
        data[n++] = c;
        if (c == '\n')
            break;
    }
    return n;
}

/*!
    Reads a line of at most \a maxSize - 1 bytes into \a data, terminates it
    with a \c{'\0'} byte, and returns the number of bytes read, not counting
    the terminator. The newline, if found, is included.

    Returns \c -1 if the device is not open for reading, and \c 0 if
    \a maxSize is less than 2 or nothing could be read.

    \sa readData(), canReadLine(), readAll()
*/
qint64 QIODevice::readLine(char *data, qint64 maxSize) {
    if (!isReadable() || maxSize < 2) {
        if (maxSize >= 1)
            data[0] = '\0';
        return isReadable() ? 0 : -1;
    }
    qint64 total = 0;
    const qint64 limit = maxSize - 1;
    while (total < limit && m_bufPos < m_buf.size()) {
        const char c = m_buf.at(m_bufPos++);
        data[total++] = c;
        if (c == '\n') {
            compactBuffer();
            data[total] = '\0';
            return total;
        }
    }
    compactBuffer();
    if (total < limit) {
        const qint64 n = readLineData(data + total, limit - total);
        if (n > 0) {
            keepForTransaction(data + total, n);
            total += n;
        }
    }
    data[total] = '\0';
    return total;
}

/*!
    \overload

    Reads a line from the device, but no more than \a maxSize bytes, and
    returns the result as a byte array. If \a maxSize is \c 0, the line length
    is not limited. The newline, if found, is included.
*/
QByteArray QIODevice::readLine(qint64 maxSize) {
    QByteArray r(static_cast<const char *>(""), 0);
    char buf[4096];
    qint64 remaining = maxSize > 0 ? maxSize : std::numeric_limits<qint64>::max();
    while (remaining > 0) {
        const qint64 want = std::min<qint64>(remaining, sizeof buf - 1) + 1;
        const qint64 n = readLine(buf, want);
        if (n <= 0)
            break;
        r.append(buf, n);
        remaining -= n;
        if (buf[n - 1] == '\n')
            break;
    }
    return r;
}

/*!
    Writes at most \a maxSize bytes of data from \a data to the device, and
    returns the number of bytes written. Returns \c -1 if the device is not
    open for writing or if an error occurs, and \c 0 if \a maxSize is not
    positive.

    \sa read(), writeData(), putChar()
*/
qint64 QIODevice::write(const char *data, qint64 maxSize) {
    if (!isWritable()) {
        setErrorString(QString::fromUtf8("device not open for writing"));
        return -1;
    }
    if (maxSize <= 0)
        return 0;
    return writeData(data, maxSize);
}

/*!
    Reads at most \a maxSize bytes from the device into \a data, without
    consuming them, and returns the number of bytes read. Returns \c -1 if
    the device is not open for reading.

    Random-access devices read and then seek back; sequential devices keep
    the bytes in a look-ahead buffer.

    \sa read(), ungetChar()
*/
qint64 QIODevice::peek(char *data, qint64 maxSize) {
    if (!isReadable() || maxSize <= 0)
        return isReadable() ? 0 : -1;
    if (!isSequential()) {
        const qint64 p = pos();
        const qint64 n = read(data, maxSize);
        seek(p);
        return n;
    }
    while (bufferedBytes() < maxSize) {
        char tmp[4096];
        const qint64 want = std::min<qint64>(sizeof tmp, maxSize - bufferedBytes());
        const qint64 n = readData(tmp, want);
        if (n <= 0)
            break;
        m_buf.append(tmp, n);
    }
    const qint64 n = std::min(bufferedBytes(), maxSize);
    std::copy(m_buf.constData() + m_bufPos, m_buf.constData() + m_bufPos + n, data);
    return n;
}

/*!
    \overload

    Reads at most \a maxSize bytes from the device without consuming them, and
    returns them as a byte array.
*/
QByteArray QIODevice::peek(qint64 maxSize) {
    QByteArray r(maxSize < 0 ? 0 : maxSize, '\0');
    const qint64 n = peek(r.data(), maxSize);
    r.resize(n < 0 ? 0 : n);
    return r;
}

/*!
    Puts the character \a c back into the device, so that the next read
    returns it. Does nothing if the device is not open for reading.

    On a random-access device this seeks back one byte and does not modify the
    stored data, so \a c should be the byte that was just read.

    \sa getChar(), peek()
*/
void QIODevice::ungetChar(char c) {
    if (!isReadable())
        return;
    if (!isSequential()) {
        seek(pos() - 1);
        return;
    }
    if (m_bufPos > 0) {
        m_buf[--m_bufPos] = c;
        if (m_transaction && m_bufPos < m_transactionPos)
            m_transactionPos = m_bufPos;
    } else {
        m_buf.prepend(c);
    }
}

/*!
    Reads one character from the device and stores it in \a c, unless \a c is
    \c nullptr. Returns \c true on success; otherwise returns \c false.

    \sa putChar(), ungetChar(), read()
*/
bool QIODevice::getChar(char *c) {
    char ch;
    if (read(&ch, 1) != 1)
        return false;
    if (c)
        *c = ch;
    return true;
}

// While a transaction runs on a sequential device, bytes read past the look-ahead buffer are
// appended to it as already consumed, so that rollbackTransaction() can return them.
void QIODevice::keepForTransaction(const char *data, qint64 n) {
    if (!m_transaction || !isSequential())
        return;
    m_buf.append(data, n);
    m_bufPos = m_buf.size();
}

/*!
    \since 6.9
    Reads a line from the device into \a line, replacing its contents, but no
    more than \a maxSize bytes; a \a maxSize of \c 0 does not limit the line
    length. The newline, if found, is included. If \a line is \c nullptr, the
    line is read and discarded.

    Returns \c true if any data was read; otherwise returns \c false and
    leaves \a line empty.

    \sa readLine(), canReadLine()
*/
bool QIODevice::readLineInto(QByteArray *line, qint64 maxSize) {
    QByteArray r = readLine(maxSize);
    const bool ok = !r.isEmpty();
    if (line)
        *line = std::move(r);
    return ok;
}

/*!
    \since 5.10
    Skips up to \a maxSize bytes of data and returns the number of bytes
    actually skipped, or \c -1 if the device is not open for reading or an
    error occurred. Look-ahead data is skipped first; the rest is passed to
    skipData().

    \sa read(), peek(), skipData()
*/
qint64 QIODevice::skip(qint64 maxSize) {
    if (!isReadable()) {
        setErrorString(QString::fromUtf8("device not open for reading"));
        return -1;
    }
    if (maxSize <= 0)
        return 0;
    qint64 total = 0;
    if (isSequential()) {
        total = std::min(bufferedBytes(), maxSize);
        m_bufPos += total;
        compactBuffer();
        if (total == maxSize)
            return total;
    }
    const qint64 n = skipData(maxSize - total);
    if (n < 0)
        return total > 0 ? total : -1;
    return total + n;
}

/*!
    \since 6.0
    Skips up to \a maxSize bytes and returns the number of bytes skipped, or
    \c -1 on error. skip() calls this function after it has skipped buffered
    look-ahead data.

    The base implementation seeks forward on a random-access device, without
    moving past size(), and reads and discards the data on a sequential
    device. Subclasses reimplement it when they can skip data more cheaply.

    \sa skip()
*/
qint64 QIODevice::skipData(qint64 maxSize) {
    if (!isSequential()) {
        const qint64 from = pos();
        const qint64 n = std::max<qint64>(0, std::min(maxSize, size() - from));
        if (n > 0 && !seek(from + n))
            return -1;
        return n;
    }
    char buf[4096];
    qint64 total = 0;
    while (total < maxSize) {
        const qint64 n = read(buf, std::min<qint64>(sizeof buf, maxSize - total));
        if (n < 0)
            return total > 0 ? total : -1;
        if (n == 0)
            break;
        total += n;
    }
    return total;
}

/*!
    \since 5.7
    Starts a read transaction on the device.

    Inside a transaction, data read from the device is kept so that
    rollbackTransaction() can restore the read position to where the
    transaction started. Call commitTransaction() to accept the data that
    was read. This is useful for parsing a message that may not have arrived
    completely:

    \code
    socket->startTransaction();
    QByteArray header = socket->read(8);
    if (header.size() < 8) {
        socket->rollbackTransaction(); // wait for more data
        return;
    }
    socket->commitTransaction();
    \endcode

    Starting a transaction while one is in progress prints a warning and does
    nothing. Writing is not affected by transactions.

    \sa commitTransaction(), rollbackTransaction(), isTransactionStarted()
*/
void QIODevice::startTransaction() {
    if (m_transaction) {
        qWarning("QIODevice::startTransaction: Called while transaction already in progress");
        return;
    }
    m_transactionPos = isSequential() ? m_bufPos : pos();
    m_transaction = true;
}

/*!
    \since 5.7
    Completes a read transaction. The data read since startTransaction() is
    consumed. Prints a warning if no transaction is in progress.

    \sa startTransaction(), rollbackTransaction()
*/
void QIODevice::commitTransaction() {
    if (!m_transaction) {
        qWarning("QIODevice::commitTransaction: Called while no transaction in progress");
        return;
    }
    m_transaction = false;
    if (isSequential() && m_bufPos > 0) {
        m_buf.remove(0, m_bufPos);
        m_bufPos = 0;
    }
    compactBuffer();
}

/*!
    \since 5.7
    Rolls back a read transaction, restoring the read position to where
    startTransaction() was called, so that the same data is read again.
    Prints a warning if no transaction is in progress.

    \sa startTransaction(), commitTransaction()
*/
void QIODevice::rollbackTransaction() {
    if (!m_transaction) {
        qWarning("QIODevice::rollbackTransaction: Called while no transaction in progress");
        return;
    }
    m_transaction = false;
    if (isSequential()) {
        m_bufPos = m_transactionPos;
        compactBuffer();
    } else {
        seek(m_transactionPos);
    }
}

#include "moc_qiodevice.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
