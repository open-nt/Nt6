// MIT License - see LICENSE
#include <QtCore/qbitarray.h>
#include <QtCore/qdatastream.h>

#include <algorithm>

/*!
    \since 5.11
    Returns the bits packed into bytes: bit \c i is bit \c{i % 8} of byte
    \c{i / 8}, counting from the least significant bit. Unused bits of the
    last byte are 0. The pointer stays valid until the array is changed or
    bits() is called again.

    \sa fromBits(), size()
*/
const char *QBitArray::bits() const {
    m_bytes.assign((d.size() + 7) / 8, '\0');
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (d[i])
            m_bytes[i / 8] = static_cast<char>(m_bytes[i / 8] | (1 << (i % 8)));
    }
    return m_bytes.empty() ? "" : m_bytes.data();
}

/*!
    \since 5.11
    Returns a bit array of \a size bits read from \a data, in the layout that
    bits() produces. \a data must hold at least \c{(size + 7) / 8} bytes.

    \sa bits()
*/
QBitArray QBitArray::fromBits(const char *data, qsizetype size) {
    QBitArray result(size < 0 ? 0 : size);
    for (qsizetype i = 0; i < result.size(); ++i) {
        const auto byte = static_cast<unsigned char>(data[i / 8]);
        result.d[static_cast<std::size_t>(i)] = ((byte >> (i % 8)) & 1) != 0;
    }
    return result;
}

/*!
    \since 6.0
    Returns the bits as a 32-bit number. With \c QSysInfo::LittleEndian
    \a endianness, bit \c i of the array is bit \c i of the number; with
    \c QSysInfo::BigEndian, the first bit of the array is the most significant
    of the size() bits. If the array has more than 32 bits, returns 0 and sets
    \c{*ok} to \c false; otherwise sets \c{*ok} to \c true, if \a ok is not
    \c nullptr.
*/
quint32 QBitArray::toUInt32(QSysInfo::Endian endianness, bool *ok) const noexcept {
    const std::size_t n = d.size();
    if (n > 32) {
        if (ok)
            *ok = false;
        return 0;
    }
    quint32 value = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t index = endianness == QSysInfo::LittleEndian ? i : n - i - 1;
        if (d[index])
            value |= quint32(1) << i;
    }
    if (ok)
        *ok = true;
    return value;
}

QDataStream &operator<<(QDataStream &out, const QBitArray &ba) {
    if (out.version() < QDataStream::Qt_6_0) {
        if (ba.size() > 0xFFFFFFFFLL) {
            out.setStatus(QDataStream::SizeLimitExceeded);
            return out;
        }
        out << static_cast<quint32>(ba.size());
    } else {
        out << static_cast<qint64>(ba.size());
    }
    const char *bytes = ba.bits();
    qsizetype left = (ba.size() + 7) / 8;
    while (left > 0 && out.status() == QDataStream::Ok) {
        const int chunk = static_cast<int>(std::min<qsizetype>(left, 1 << 20));
        if (out.writeRawData(bytes, chunk) != chunk)
            out.setStatus(QDataStream::WriteFailed);
        bytes += chunk;
        left -= chunk;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QBitArray &ba) {
    ba.clear();
    qint64 bitCount = 0;
    if (in.version() < QDataStream::Qt_6_0) {
        quint32 n = 0;
        in >> n;
        bitCount = n;
    } else {
        in >> bitCount;
    }
    if (in.status() != QDataStream::Ok)
        return in;
    if (bitCount < 0) {
        in.setStatus(QDataStream::ReadCorruptData);
        return in;
    }
    // Read in chunks, so that a corrupt size cannot allocate more than the stream holds.
    std::vector<char> bytes;
    qint64 left = (bitCount + 7) / 8;
    while (left > 0) {
        const int chunk = static_cast<int>(std::min<qint64>(left, 1 << 16));
        const std::size_t at = bytes.size();
        bytes.resize(at + static_cast<std::size_t>(chunk));
        if (in.readRawData(bytes.data() + at, chunk) != chunk) {
            in.setStatus(QDataStream::ReadPastEnd);
            return in;
        }
        left -= chunk;
    }
    ba = QBitArray::fromBits(bytes.data(), static_cast<qsizetype>(bitCount));
    return in;
}
