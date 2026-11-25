// MIT License - see LICENSE
#pragma once

#include <QtProtobuf/qabstractprotobufserializer.h>

class QProtobufSerializer : public QAbstractProtobufSerializer {
public:
    QProtobufSerializer();
    ~QProtobufSerializer() override;

    Error lastError() const override;
    QString lastErrorString() const override;
    void shouldPreserveUnknownFields(bool preserveUnknownFields);

private:
    QByteArray serializeMessage(const QProtobufMessage *message) const override;
    bool deserializeMessage(QProtobufMessage *message, QByteArrayView data) const override;

    mutable Error m_error = Error::None;
    mutable QString m_errorString;
    bool m_preserveUnknown = true;
};
