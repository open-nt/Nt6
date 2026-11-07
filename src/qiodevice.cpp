// MIT License - see LICENSE
#include <QtCore/qiodevice.h>

#include <algorithm>

QIODevice::QIODevice() = default;

QIODevice::QIODevice(QObject *parent) : QObject(parent) {}

QIODevice::~QIODevice() = default;

void QIODevice::setTextModeEnabled(bool enabled) {
    if (enabled)
        m_mode |= Text;
    else
        m_mode &= ~OpenMode(Text);
}

bool QIODevice::open(OpenMode mode) {
    m_mode = mode;
    m_buf.clear();
    m_bufPos = 0;
    m_error.clear();
    return true;
}

void QIODevice::close() {
    if (!isOpen())
        return;
    aboutToClose();
    m_mode = NotOpen;
    m_buf.clear();
    m_bufPos = 0;
}

bool QIODevice::seek(qint64) {
    m_buf.clear();
    m_bufPos = 0;
    return true;
}

bool QIODevice::atEnd() const { return isOpen() && bytesAvailable() <= 0; }

qint64 QIODevice::bytesAvailable() const { return bufferedBytes(); }

bool QIODevice::canReadLine() const {
    return m_buf.indexOf('\n', m_bufPos) != -1;
}

QString QIODevice::errorString() const { return m_error.isEmpty() ? QString::fromUtf8("Unknown error") : m_error; }

void QIODevice::compactBuffer() {
    if (m_bufPos >= m_buf.size()) {
        m_buf.clear();
        m_bufPos = 0;
    }
}

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
        if (n > 0)
            total += n;
        else if (n < 0 && total == 0)
            return -1;
    }
    return total;
}

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
        if (n > 0)
            total += n;
    }
    data[total] = '\0';
    return total;
}

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

qint64 QIODevice::write(const char *data, qint64 maxSize) {
    if (!isWritable()) {
        setErrorString(QString::fromUtf8("device not open for writing"));
        return -1;
    }
    if (maxSize <= 0)
        return 0;
    return writeData(data, maxSize);
}

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

QByteArray QIODevice::peek(qint64 maxSize) {
    QByteArray r(maxSize < 0 ? 0 : maxSize, '\0');
    const qint64 n = peek(r.data(), maxSize);
    r.resize(n < 0 ? 0 : n);
    return r;
}

void QIODevice::ungetChar(char c) {
    if (!isReadable())
        return;
    if (!isSequential()) {
        seek(pos() - 1);
        return;
    }
    if (m_bufPos > 0)
        m_buf[--m_bufPos] = c;
    else
        m_buf.prepend(c);
}

bool QIODevice::getChar(char *c) {
    char ch;
    if (read(&ch, 1) != 1)
        return false;
    if (c)
        *c = ch;
    return true;
}
