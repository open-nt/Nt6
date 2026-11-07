// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>

#include <memory>
#include <type_traits>

// Binary format compatible with Qt's QDataStream: big-endian by default, QString as UTF-16 code
// units behind a byte-length prefix, null QString/QByteArray as length 0xFFFFFFFF. Container sizes
// are always quint32 (the 64-bit extended size of stream version 21 is not supported).
class QDataStream {
public:
    enum Version {
        Qt_1_0 = 1,
        Qt_2_0 = 2,
        Qt_2_1 = 3,
        Qt_3_0 = 4,
        Qt_3_1 = 5,
        Qt_3_3 = 6,
        Qt_4_0 = 7,
        Qt_4_1 = Qt_4_0,
        Qt_4_2 = 8,
        Qt_4_3 = 9,
        Qt_4_4 = 10,
        Qt_4_5 = 11,
        Qt_4_6 = 12,
        Qt_4_7 = Qt_4_6,
        Qt_4_8 = Qt_4_7,
        Qt_4_9 = Qt_4_8,
        Qt_5_0 = 13,
        Qt_5_1 = 14,
        Qt_5_2 = 15,
        Qt_5_3 = Qt_5_2,
        Qt_5_4 = 16,
        Qt_5_5 = Qt_5_4,
        Qt_5_6 = 17,
        Qt_5_13 = 19,
        Qt_5_14 = Qt_5_13,
        Qt_5_15 = Qt_5_14,
        Qt_6_0 = 20,
        Qt_6_1 = Qt_6_0,
        Qt_6_2 = Qt_6_0,
        Qt_6_3 = Qt_6_0,
        Qt_6_4 = Qt_6_0,
        Qt_6_5 = Qt_6_0,
        Qt_6_6 = Qt_6_0,
        Qt_6_7 = 21,
        Qt_DefaultCompiledVersion = Qt_6_7
    };
    enum ByteOrder { BigEndian, LittleEndian };
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed, SizeLimitExceeded };
    enum FloatingPointPrecision { SinglePrecision, DoublePrecision };

    QDataStream() = default;
    explicit QDataStream(QIODevice *d) : dev(d) {}
    QDataStream(QByteArray *array, QIODeviceBase::OpenMode mode);
    explicit QDataStream(const QByteArray &data);
    ~QDataStream();

    QIODevice *device() const { return dev; }
    void setDevice(QIODevice *d);
    bool atEnd() const { return !dev || dev->atEnd(); }
    Status status() const { return st; }
    void setStatus(Status status) { if (st == Ok) st = status; }
    void resetStatus() { st = Ok; }
    ByteOrder byteOrder() const { return order; }
    void setByteOrder(ByteOrder o) { order = o; }
    FloatingPointPrecision floatingPointPrecision() const { return precision; }
    void setFloatingPointPrecision(FloatingPointPrecision p) { precision = p; }
    int version() const { return ver; }
    void setVersion(int v) { ver = v; }

    QDataStream &operator>>(qint8 &v);
    QDataStream &operator>>(quint8 &v) { return *this >> reinterpret_cast<qint8 &>(v); }
    QDataStream &operator>>(qint16 &v);
    QDataStream &operator>>(quint16 &v) { return *this >> reinterpret_cast<qint16 &>(v); }
    QDataStream &operator>>(qint32 &v);
    QDataStream &operator>>(quint32 &v) { return *this >> reinterpret_cast<qint32 &>(v); }
    QDataStream &operator>>(qint64 &v);
    QDataStream &operator>>(quint64 &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    QDataStream &operator>>(char &v) { return *this >> reinterpret_cast<qint8 &>(v); }
    // qint64 is `long` on LP64 Linux, so the `long long` family needs its own overloads.
    template <typename T, std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, int> = 0>
    QDataStream &operator>>(T &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    template <typename T, std::enable_if_t<std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, quint64>, int> = 0>
    QDataStream &operator>>(T &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    QDataStream &operator>>(bool &v);
    QDataStream &operator>>(float &v);
    QDataStream &operator>>(double &v);
    QDataStream &operator>>(char *&s);
    QDataStream &operator<<(qint8 v);
    QDataStream &operator<<(quint8 v) { return *this << static_cast<qint8>(v); }
    QDataStream &operator<<(qint16 v);
    QDataStream &operator<<(quint16 v) { return *this << static_cast<qint16>(v); }
    QDataStream &operator<<(qint32 v);
    QDataStream &operator<<(quint32 v) { return *this << static_cast<qint32>(v); }
    QDataStream &operator<<(qint64 v);
    QDataStream &operator<<(quint64 v) { return *this << static_cast<qint64>(v); }
    QDataStream &operator<<(char v) { return *this << static_cast<qint8>(v); }
    template <typename T, std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, int> = 0>
    QDataStream &operator<<(T v) { return *this << static_cast<qint64>(v); }
    template <typename T, std::enable_if_t<std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, quint64>, int> = 0>
    QDataStream &operator<<(T v) { return *this << static_cast<qint64>(v); }
    QDataStream &operator<<(bool v) { return *this << static_cast<qint8>(v); }
    QDataStream &operator<<(float v);
    QDataStream &operator<<(double v);
    QDataStream &operator<<(const char *s);

    QDataStream &readBytes(char *&s, qint64 &len);
    int readRawData(char *s, int len);
    QDataStream &writeBytes(const char *s, qint64 len);
    int writeRawData(const char *s, int len);
    int skipRawData(int len);

private:
    bool readSwapped(void *out, int size);
    QDataStream &writeSwapped(const void *in, int size);

    QIODevice *dev = nullptr;
    std::unique_ptr<QIODevice> owned;
    ByteOrder order = BigEndian;
    Status st = Ok;
    FloatingPointPrecision precision = DoublePrecision;
    int ver = Qt_DefaultCompiledVersion;
};

QDataStream &operator<<(QDataStream &out, const QByteArray &ba);
QDataStream &operator>>(QDataStream &in, QByteArray &ba);
QDataStream &operator<<(QDataStream &out, const QString &s);
QDataStream &operator>>(QDataStream &in, QString &s);
QDataStream &operator<<(QDataStream &out, QChar c);
QDataStream &operator>>(QDataStream &in, QChar &c);

template <typename T>
QDataStream &operator<<(QDataStream &out, const QList<T> &l) {
    out << static_cast<quint32>(l.size());
    for (const T &t : l)
        out << t;
    return out;
}

template <typename T>
QDataStream &operator>>(QDataStream &in, QList<T> &l) {
    l.clear();
    quint32 n = 0;
    in >> n;
    for (quint32 i = 0; i < n && in.status() == QDataStream::Ok; ++i) {
        T t;
        in >> t;
        if (in.status() == QDataStream::Ok)
            l.append(std::move(t));
    }
    return in;
}

template <typename K, typename V>
QDataStream &operator<<(QDataStream &out, const QMap<K, V> &m) {
    out << static_cast<quint32>(m.size());
    for (auto it = m.begin(); it != m.end(); ++it)
        out << it.key() << it.value();
    return out;
}

template <typename K, typename V>
QDataStream &operator>>(QDataStream &in, QMap<K, V> &m) {
    m.clear();
    quint32 n = 0;
    in >> n;
    for (quint32 i = 0; i < n && in.status() == QDataStream::Ok; ++i) {
        K k;
        V v;
        in >> k >> v;
        if (in.status() == QDataStream::Ok)
            m.insert(k, v);
    }
    return in;
}

template <typename K, typename V>
QDataStream &operator<<(QDataStream &out, const QHash<K, V> &m) {
    out << static_cast<quint32>(m.size());
    for (auto it = m.begin(); it != m.end(); ++it)
        out << it.key() << it.value();
    return out;
}

template <typename K, typename V>
QDataStream &operator>>(QDataStream &in, QHash<K, V> &m) {
    m.clear();
    quint32 n = 0;
    in >> n;
    for (quint32 i = 0; i < n && in.status() == QDataStream::Ok; ++i) {
        K k;
        V v;
        in >> k >> v;
        if (in.status() == QDataStream::Ok)
            m.insert(k, v);
    }
    return in;
}
