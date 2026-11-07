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
    /*!
        \enum QDataStream::Version

        Selects the version of the stream format, which determines how some
        types are encoded. Streams default to \c Qt_DefaultCompiledVersion.
        The names are those of the Qt release that introduced the format;
        versions that share a format share a value.

        \value Qt_1_0 Version 1 (Qt 1.x).
        \value Qt_2_0 Version 2 (Qt 2.0).
        \value Qt_2_1 Version 3 (Qt 2.1).
        \value Qt_3_0 Version 4 (Qt 3.0).
        \value Qt_3_1 Version 5 (Qt 3.1).
        \value Qt_3_3 Version 6 (Qt 3.3).
        \value Qt_4_0 Version 7 (Qt 4.0).
        \value Qt_4_1 Same as \c Qt_4_0.
        \value Qt_4_2 Version 8 (Qt 4.2).
        \value Qt_4_3 Version 9 (Qt 4.3).
        \value Qt_4_4 Version 10 (Qt 4.4).
        \value Qt_4_5 Version 11 (Qt 4.5).
        \value Qt_4_6 Version 12 (Qt 4.6). From this version, floating-point
               values honor floatingPointPrecision().
        \value Qt_4_7 Same as \c Qt_4_6.
        \value Qt_4_8 Same as \c Qt_4_7.
        \value Qt_4_9 Same as \c Qt_4_8.
        \value Qt_5_0 Version 13 (Qt 5.0).
        \value Qt_5_1 Version 14 (Qt 5.1).
        \value Qt_5_2 Version 15 (Qt 5.2).
        \value Qt_5_3 Same as \c Qt_5_2.
        \value Qt_5_4 Version 16 (Qt 5.4).
        \value Qt_5_5 Same as \c Qt_5_4.
        \value Qt_5_6 Version 17 (Qt 5.6).
        \value Qt_5_13 Version 19 (Qt 5.13).
        \value Qt_5_14 Same as \c Qt_5_13.
        \value Qt_5_15 Same as \c Qt_5_14.
        \value Qt_6_0 Version 20 (Qt 6.0).
        \value Qt_6_1 Same as \c Qt_6_0.
        \value Qt_6_2 Same as \c Qt_6_0.
        \value Qt_6_3 Same as \c Qt_6_0.
        \value Qt_6_4 Same as \c Qt_6_0.
        \value Qt_6_5 Same as \c Qt_6_0.
        \value Qt_6_6 Same as \c Qt_6_0.
        \value Qt_6_7 Version 21 (Qt 6.7).
        \value Qt_DefaultCompiledVersion The version used by default, which is
               \c Qt_6_7.
    */
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
    /*!
        \enum QDataStream::ByteOrder

        Describes the byte order used to encode multi-byte numbers.

        \value BigEndian The most significant byte comes first. This is the
               default.
        \value LittleEndian The least significant byte comes first.
    */
    enum ByteOrder { BigEndian, LittleEndian };
    /*!
        \enum QDataStream::Status

        Describes the state of the stream.

        \value Ok The stream is operating normally.
        \value ReadPastEnd The stream has read past the end of the data in the
               device.
        \value ReadCorruptData The stream read corrupt data, such as a
               string with an odd byte length.
        \value WriteFailed The stream cannot write to the underlying device.
        \value SizeLimitExceeded The stream cannot write the data because its
               size exceeds the format's 32-bit limit.
    */
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed, SizeLimitExceeded };
    /*!
        \enum QDataStream::FloatingPointPrecision

        Selects the precision of floating-point values in the stream, for
        versions \c Qt_4_6 and later.

        \value SinglePrecision \c float and \c double values are written as
               32-bit numbers.
        \value DoublePrecision \c float and \c double values are written as
               64-bit numbers. This is the default.
    */
    enum FloatingPointPrecision { SinglePrecision, DoublePrecision };

    /*!
        Constructs a data stream that has no I/O device.

        \sa setDevice()
    */
    QDataStream() = default;
    /*!
        Constructs a data stream that uses the I/O device \a d. The stream
        does not take ownership of the device.

        \sa setDevice(), device()
    */
    explicit QDataStream(QIODevice *d) : dev(d) {}
    QDataStream(QByteArray *array, QIODeviceBase::OpenMode mode);
    explicit QDataStream(const QByteArray &data);
    ~QDataStream();

    /*!
        Returns the I/O device currently set, or \c nullptr if no device is
        set.

        \sa setDevice()
    */
    QIODevice *device() const { return dev; }
    void setDevice(QIODevice *d);
    /*!
        Returns \c true if the I/O device has reached the end position, or if
        there is no device set; otherwise returns \c false.

        \sa QIODevice::atEnd()
    */
    bool atEnd() const { return !dev || dev->atEnd(); }
    /*!
        Returns the status of the data stream.

        \sa Status, setStatus(), resetStatus()
    */
    Status status() const { return st; }
    /*!
        Sets the status of the data stream to \a status, unless an error was
        already recorded. The first error is kept until resetStatus() is
        called.

        \sa Status, status(), resetStatus()
    */
    void setStatus(Status status) { if (st == Ok) st = status; }
    /*!
        Resets the status of the data stream to \c Ok.

        \sa status(), setStatus()
    */
    void resetStatus() { st = Ok; }
    /*!
        Returns the current byte order setting, which is \c BigEndian unless
        changed.

        \sa setByteOrder()
    */
    ByteOrder byteOrder() const { return order; }
    /*!
        Sets the byte order of the stream to \a o. It should be set before
        any data is read or written.

        \sa byteOrder()
    */
    void setByteOrder(ByteOrder o) { order = o; }
    /*!
        Returns the floating-point precision of the stream.

        \sa FloatingPointPrecision, setFloatingPointPrecision()
    */
    FloatingPointPrecision floatingPointPrecision() const { return precision; }
    /*!
        Sets the floating-point precision of the stream to \a p. It affects
        stream versions \c Qt_4_6 and later.

        \sa FloatingPointPrecision, floatingPointPrecision()
    */
    void setFloatingPointPrecision(FloatingPointPrecision p) { precision = p; }
    /*!
        Returns the version number of the data serialization format.

        \sa setVersion(), Version
    */
    int version() const { return ver; }
    /*!
        Sets the version number of the data serialization format to \a v,
        one of the Version values.

        \sa version(), Version
    */
    void setVersion(int v) { ver = v; }

    /*!
        Reads a signed byte from the stream into \a v and returns a reference
        to the stream. If the data cannot be read, \a v is set to \c 0 and the
        status is set to \c ReadPastEnd.

        \sa status()
    */
    QDataStream &operator>>(qint8 &v);
    /*!
        \overload

        Reads an unsigned byte from the stream into \a v.
    */
    QDataStream &operator>>(quint8 &v) { return *this >> reinterpret_cast<qint8 &>(v); }
    /*!
        \overload

        Reads a signed 16-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(qint16 &v);
    /*!
        \overload

        Reads an unsigned 16-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(quint16 &v) { return *this >> reinterpret_cast<qint16 &>(v); }
    /*!
        \overload

        Reads a signed 32-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(qint32 &v);
    /*!
        \overload

        Reads an unsigned 32-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(quint32 &v) { return *this >> reinterpret_cast<qint32 &>(v); }
    /*!
        \overload

        Reads a signed 64-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(qint64 &v);
    /*!
        \overload

        Reads an unsigned 64-bit integer from the stream into \a v, using the
        stream's byte order.
    */
    QDataStream &operator>>(quint64 &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    /*!
        \overload

        Reads a byte from the stream into the character \a v.
    */
    QDataStream &operator>>(char &v) { return *this >> reinterpret_cast<qint8 &>(v); }
    // qint64 is `long` on LP64 Linux, so the `long long` family needs its own overloads.
    /*!
        \internal
    */
    template <typename T, std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, int> = 0>
    QDataStream &operator>>(T &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    /*!
        \internal
    */
    template <typename T, std::enable_if_t<std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, quint64>, int> = 0>
    QDataStream &operator>>(T &v) { return *this >> reinterpret_cast<qint64 &>(v); }
    /*!
        \overload

        Reads a boolean from the stream into \a v. A byte other than \c 0 is
        read as \c true.
    */
    QDataStream &operator>>(bool &v);
    /*!
        \overload

        Reads a floating-point value from the stream into \a v. With a
        version of \c Qt_4_6 or later and the default double precision, a
        64-bit value is read and narrowed to \c float.

        \sa setFloatingPointPrecision()
    */
    QDataStream &operator>>(float &v);
    /*!
        \overload

        Reads a floating-point value from the stream into \a v. With
        single precision and a version of \c Qt_4_6 or later, a 32-bit value
        is read and widened to \c double.

        \sa setFloatingPointPrecision()
    */
    QDataStream &operator>>(double &v);
    /*!
        \overload

        Reads a length-prefixed byte string from the stream, allocates a
        buffer with \c{new char[]} for it and stores the pointer in \a s. The
        string is null-terminated. The caller must free it with
        \c{delete[]}. \a s is \c nullptr if the string is empty or an error
        occurs.

        \sa readBytes()
    */
    QDataStream &operator>>(char *&s);
    /*!
        Writes a signed byte \a v to the stream and returns a reference to the
        stream. If the data cannot be written, the status is set to
        \c WriteFailed.

        \sa status()
    */
    QDataStream &operator<<(qint8 v);
    /*!
        \overload

        Writes an unsigned byte \a v to the stream.
    */
    QDataStream &operator<<(quint8 v) { return *this << static_cast<qint8>(v); }
    /*!
        \overload

        Writes a signed 16-bit integer \a v to the stream, using the stream's
        byte order.
    */
    QDataStream &operator<<(qint16 v);
    /*!
        \overload

        Writes an unsigned 16-bit integer \a v to the stream, using the
        stream's byte order.
    */
    QDataStream &operator<<(quint16 v) { return *this << static_cast<qint16>(v); }
    /*!
        \overload

        Writes a signed 32-bit integer \a v to the stream, using the stream's
        byte order.
    */
    QDataStream &operator<<(qint32 v);
    /*!
        \overload

        Writes an unsigned 32-bit integer \a v to the stream, using the
        stream's byte order.
    */
    QDataStream &operator<<(quint32 v) { return *this << static_cast<qint32>(v); }
    /*!
        \overload

        Writes a signed 64-bit integer \a v to the stream, using the stream's
        byte order.
    */
    QDataStream &operator<<(qint64 v);
    /*!
        \overload

        Writes an unsigned 64-bit integer \a v to the stream, using the
        stream's byte order.
    */
    QDataStream &operator<<(quint64 v) { return *this << static_cast<qint64>(v); }
    /*!
        \overload

        Writes the character \a v to the stream as a single byte.
    */
    QDataStream &operator<<(char v) { return *this << static_cast<qint8>(v); }
    /*!
        \internal
    */
    template <typename T, std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, int> = 0>
    QDataStream &operator<<(T v) { return *this << static_cast<qint64>(v); }
    /*!
        \internal
    */
    template <typename T, std::enable_if_t<std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, quint64>, int> = 0>
    QDataStream &operator<<(T v) { return *this << static_cast<qint64>(v); }
    /*!
        \overload

        Writes the boolean \a v to the stream as a single byte, \c 0 or \c 1.
    */
    QDataStream &operator<<(bool v) { return *this << static_cast<qint8>(v); }
    /*!
        \overload

        Writes the floating-point value \a v to the stream. With a version of
        \c Qt_4_6 or later and the default double precision, it is written as
        a 64-bit value.

        \sa setFloatingPointPrecision()
    */
    QDataStream &operator<<(float v);
    /*!
        \overload

        Writes the floating-point value \a v to the stream. With single
        precision and a version of \c Qt_4_6 or later, it is written as a
        32-bit value.

        \sa setFloatingPointPrecision()
    */
    QDataStream &operator<<(double v);
    /*!
        \overload

        Writes the null-terminated string \a s to the stream as a 32-bit
        length, which includes the terminating null byte, followed by the
        bytes. A null pointer is written as a length of \c 0.

        \sa writeBytes()
    */
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

template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
QDataStream &operator<<(QDataStream &out, Enum e) { return out << static_cast<std::underlying_type_t<Enum>>(e); }

template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
QDataStream &operator>>(QDataStream &in, Enum &e) {
    std::underlying_type_t<Enum> v{};
    in >> v;
    e = static_cast<Enum>(v);
    return in;
}

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
    for (quint32 i = 0; i < n; ++i) {
        T t{};
        in >> t;
        if (in.status() != QDataStream::Ok) {
            l.clear();
            break;
        }
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
    for (quint32 i = 0; i < n; ++i) {
        K k{};
        V v{};
        in >> k >> v;
        if (in.status() != QDataStream::Ok) {
            m.clear();
            break;
        }
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
    for (quint32 i = 0; i < n; ++i) {
        K k{};
        V v{};
        in >> k >> v;
        if (in.status() != QDataStream::Ok) {
            m.clear();
            break;
        }
        m.insert(k, v);
    }
    return in;
}
