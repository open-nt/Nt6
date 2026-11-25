// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qstring.h>

class QProtobufMessage;

class QAbstractProtobufSerializer {
public:
    enum class Error { None, InvalidHeader, UnknownType, UnexpectedEndOfStream, InvalidFormat };

    QAbstractProtobufSerializer();
    virtual ~QAbstractProtobufSerializer();
    QAbstractProtobufSerializer(const QAbstractProtobufSerializer &) = delete;
    QAbstractProtobufSerializer &operator=(const QAbstractProtobufSerializer &) = delete;

    QByteArray serialize(const QProtobufMessage *message) const;
    bool deserialize(QProtobufMessage *message, QByteArrayView data) const;

    virtual Error lastError() const = 0;
    virtual QString lastErrorString() const = 0;

private:
    virtual QByteArray serializeMessage(const QProtobufMessage *message) const = 0;
    virtual bool deserializeMessage(QProtobufMessage *message, QByteArrayView data) const = 0;
};
