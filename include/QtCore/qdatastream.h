// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>

#include <memory>

class QDataStream {
public:
    enum Version {
        Qt_5_0 = 13,
        Qt_5_15 = 20,
        Qt_6_0 = 20,
        Qt_6_5 = 21,
        Qt_DefaultCompiledVersion = Qt_6_5
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
