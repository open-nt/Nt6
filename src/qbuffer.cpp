// MIT License - see LICENSE
#include <QtCore/qbuffer.h>

#include <algorithm>

/*!
    \class QBuffer
    \brief The QBuffer class provides a QIODevice interface for a QByteArray.

    QBuffer lets code that works with a QIODevice read from and write to
    memory. By default the buffer owns an internal QByteArray; pass a pointer
    to the constructor or to setBuffer() to operate on a byte array owned by
    the caller instead, which must outlive the buffer.

    \code
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    buffer.write("Hello");
    buffer.close();
    const QByteArray data = buffer.data();
    \endcode

    Writing past the end of the buffer grows the byte array. A buffer is a
    random-access device.

    \differences
    The \c Text open flag is cleared on open, and open() always succeeds.
    seek() fails for positions past the end of the buffer instead of growing
    it. bytesWritten() and readyRead() are never emitted. The length argument
    of \c setData(const char *, qint64) is a \c qint64.
    \enddifferences

    \sa QIODevice, QByteArray, QFile
*/

/*!
    Constructs a buffer that uses an internal QByteArray, with the given
    \a parent. The buffer is not open.

    \sa open(), setBuffer()
*/
QBuffer::QBuffer(QObject *parent) : QIODevice(parent), m_buf(&m_internal) {}

/*!
    Constructs a buffer that uses the QByteArray pointed to by \a byteArray,
    with the given \a parent. The caller keeps ownership of the byte array,
    which must outlive the buffer. If \a byteArray is \c nullptr, the buffer
    uses its internal byte array.

    \sa setBuffer(), buffer()
*/
QBuffer::QBuffer(QByteArray *byteArray, QObject *parent)
    : QIODevice(parent), m_buf(byteArray ? byteArray : &m_internal) {}

/*!
    Destroys the buffer.
*/
QBuffer::~QBuffer() = default;

/*!
    Makes the buffer use the QByteArray pointed to by \a a, which the caller
    keeps ownership of. If \a a is \c nullptr, the buffer uses its internal
    byte array. Does nothing if the buffer is open.

    \sa buffer(), setData()
*/
void QBuffer::setBuffer(QByteArray *a) {
    if (isOpen())
        return;
    m_buf = a ? a : &m_internal;
}

/*!
    Sets the contents of the buffer to \a data. Does nothing if the buffer is
    open.

    \sa data(), setBuffer()
*/
void QBuffer::setData(const QByteArray &data) {
    if (isOpen())
        return;
    *m_buf = data;
}

/*!
    \reimp

    Opens the buffer in the given \a mode and returns \c true. The \c Text
    flag is ignored. If \a mode includes both \c Truncate and \c WriteOnly,
    the byte array is cleared first. With \c Append the position starts at the
    end of the data; otherwise at the start.

    \sa close()
*/
bool QBuffer::open(OpenMode mode) {
    mode &= ~OpenMode(Text);
    if ((mode & Truncate) && (mode & WriteOnly))
        m_buf->clear();
    QIODevice::open(mode);
    m_pos = mode.testFlag(Append) ? m_buf->size() : 0;
    return true;
}

/*!
    \reimp

    Closes the buffer and resets the position to \c 0. The data is kept.
*/
void QBuffer::close() {
    QIODevice::close();
    m_pos = 0;
}

/*!
    \reimp

    Sets the current position to \a off. Returns \c true on success. Returns
    \c false if \a off is negative or past the end of the data.
*/
bool QBuffer::seek(qint64 off) {
    if (off < 0 || off > m_buf->size())
        return false;
    QIODevice::seek(off);
    m_pos = off;
    return true;
}

/*!
    \reimp

    Returns the number of bytes between the current position and the end of
    the data, plus any buffered look-ahead data.
*/
qint64 QBuffer::bytesAvailable() const { return QIODevice::bytesAvailable() + std::max<qint64>(0, m_buf->size() - m_pos); }

/*!
    \reimp

    Returns \c true if there is a newline between the current position and the
    end of the data; otherwise returns \c false.
*/
bool QBuffer::canReadLine() const {
    return QIODevice::canReadLine() || m_buf->indexOf('\n', m_pos) != -1;
}

/*!
    \reimp

    Copies up to \a maxSize bytes from the byte array into \a data, starting
    at the current position. Returns the number of bytes copied, which is
    \c 0 at the end of the data.
*/
qint64 QBuffer::readData(char *data, qint64 maxSize) {
    const qint64 n = std::min(maxSize, std::max<qint64>(0, m_buf->size() - m_pos));
    if (n <= 0)
        return 0;
    std::copy(m_buf->constData() + m_pos, m_buf->constData() + m_pos + n, data);
    m_pos += n;
    return n;
}

/*!
    \reimp

    Writes \a maxSize bytes from \a data into the byte array at the current
    position, growing the array if needed, and returns \a maxSize. In append
    mode, the data is always written at the end.
*/
qint64 QBuffer::writeData(const char *data, qint64 maxSize) {
    if (openMode().testFlag(Append))
        m_pos = m_buf->size();
    if (m_pos + maxSize > m_buf->size())
        m_buf->resize(m_pos + maxSize);
    std::copy(data, data + maxSize, m_buf->data() + m_pos);
    m_pos += maxSize;
    return maxSize;
}

#include "moc_qbuffer.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
