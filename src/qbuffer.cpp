// MIT License - see LICENSE
#include <QtCore/qbuffer.h>

#include <algorithm>

QBuffer::QBuffer(QObject *parent) : QIODevice(parent), m_buf(&m_internal) {}

QBuffer::QBuffer(QByteArray *byteArray, QObject *parent)
    : QIODevice(parent), m_buf(byteArray ? byteArray : &m_internal) {}

QBuffer::~QBuffer() = default;

void QBuffer::setBuffer(QByteArray *a) {
    if (isOpen())
        return;
    m_buf = a ? a : &m_internal;
}

void QBuffer::setData(const QByteArray &data) {
    if (isOpen())
        return;
    *m_buf = data;
}

bool QBuffer::open(OpenMode mode) {
    mode &= ~OpenMode(Text);
    if ((mode & Truncate) && (mode & WriteOnly))
        m_buf->clear();
    QIODevice::open(mode);
    m_pos = mode.testFlag(Append) ? m_buf->size() : 0;
    return true;
}

void QBuffer::close() {
    QIODevice::close();
    m_pos = 0;
}

bool QBuffer::seek(qint64 off) {
    if (off < 0 || off > m_buf->size())
        return false;
    QIODevice::seek(off);
    m_pos = off;
    return true;
}

qint64 QBuffer::bytesAvailable() const { return QIODevice::bytesAvailable() + std::max<qint64>(0, m_buf->size() - m_pos); }

bool QBuffer::canReadLine() const {
    return QIODevice::canReadLine() || m_buf->indexOf('\n', m_pos) != -1;
}

qint64 QBuffer::readData(char *data, qint64 maxSize) {
    const qint64 n = std::min(maxSize, std::max<qint64>(0, m_buf->size() - m_pos));
    if (n <= 0)
        return 0;
    std::copy(m_buf->constData() + m_pos, m_buf->constData() + m_pos + n, data);
    m_pos += n;
    return n;
}

qint64 QBuffer::writeData(const char *data, qint64 maxSize) {
    if (openMode().testFlag(Append))
        m_pos = m_buf->size();
    if (m_pos + maxSize > m_buf->size())
        m_buf->resize(m_pos + maxSize);
    std::copy(data, data + maxSize, m_buf->data() + m_pos);
    m_pos += maxSize;
    return maxSize;
}
