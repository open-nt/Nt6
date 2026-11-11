// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcborcommon.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <cstddef>
#include <memory>
#include <type_traits>

class QIODevice;

class QCborStreamWriter {
public:
    explicit QCborStreamWriter(QIODevice *device);
    explicit QCborStreamWriter(QByteArray *data);
    ~QCborStreamWriter();
    QCborStreamWriter(const QCborStreamWriter &) = delete;
    QCborStreamWriter &operator=(const QCborStreamWriter &) = delete;

    void setDevice(QIODevice *device);
    QIODevice *device() const;

    void append(quint64 u);
    void append(qint64 i);
    /*!
        \overload
        Writes the integer \a i, where \c{long long} is another type than \c qint64.
    */
    template <typename T,
              std::enable_if_t<std::is_same_v<T, long long> && !std::is_same_v<long long, qint64>, bool> = true>
    void append(T i) {
        append(static_cast<qint64>(i));
    }
    void append(QCborNegativeInteger n);
    void append(const QByteArray &ba);
    void append(QLatin1StringView str);
    void append(QStringView str);
    void append(QCborTag tag);
    void append(QCborKnownTags tag);
    void append(QCborSimpleType st);
    void append(std::nullptr_t);
    void append(qfloat16 f);
    void append(float f);
    void append(double value);
    void append(bool b);
    void append(int i);
    void append(uint u);
    void append(const char *str, qsizetype size = -1);

    void appendByteString(const char *data, qsizetype len);
    void appendTextString(const char *utf8, qsizetype len);
    void appendNull();
    void appendUndefined();

    void startArray();
    void startArray(quint64 count);
    bool endArray();
    void startMap();
    void startMap(quint64 count);
    bool endMap();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
