// MIT License - see LICENSE
#pragma once

#include <QtProtobuf/qabstractprotobufserializer.h>

class QProtobufJsonSerializer : public QAbstractProtobufSerializer {
public:
    QProtobufJsonSerializer();
    ~QProtobufJsonSerializer() override;

    Error lastError() const override;
    QString lastErrorString() const override;

private:
    QByteArray serializeMessage(const QProtobufMessage *message) const override;
    bool deserializeMessage(QProtobufMessage *message, QByteArrayView data) const override;

    mutable Error m_error = Error::None;
    mutable QString m_errorString;
};
