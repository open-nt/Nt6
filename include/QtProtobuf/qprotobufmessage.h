// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <memory>

class QAbstractProtobufSerializer;
class QJsonObject;
class QProtobufMessage;

namespace QtProtobufPrivate {
class Reader;
class Writer;
struct MessageAccess;

// What a generated message class tells about itself: its full name and how to make one.
struct QProtobufPropertyOrdering {
    const char *messageFullName;
    QProtobufMessage *(*create)();

    QAnyStringView getMessageFullName() const { return QAnyStringView(messageFullName); }
};
} // namespace QtProtobufPrivate

struct QProtobufMessageDeleter {
    void operator()(QProtobufMessage *message) const;
};
using QProtobufMessagePointer = std::unique_ptr<QProtobufMessage, QProtobufMessageDeleter>;

class QProtobufMessage {
public:
    virtual ~QProtobufMessage();

    QVariant property(QAnyStringView propertyName) const;
    bool setProperty(QAnyStringView propertyName, const QVariant &value);

    static QProtobufMessagePointer constructByName(const QString &messageType);

    QByteArray serialize(QAbstractProtobufSerializer *serializer) const;
    bool deserialize(QAbstractProtobufSerializer *serializer, QByteArrayView data);

    virtual const QtProtobufPrivate::QProtobufPropertyOrdering *propertyOrdering() const;

    QList<qint32> unknownFieldNumbers() const;
    QList<QByteArray> unknownFieldData(qint32 field) const;

protected:
    QProtobufMessage();
    QProtobufMessage(const QProtobufMessage &other);
    QProtobufMessage(QProtobufMessage &&other) noexcept;
    QProtobufMessage &operator=(const QProtobufMessage &other);
    QProtobufMessage &operator=(QProtobufMessage &&other) noexcept;

    // Implemented by generated classes; called by the serializers through QtProtobufPrivate::MessageAccess.
    virtual void ntClear();
    virtual void ntEncode(QtProtobufPrivate::Writer &writer) const;
    virtual bool ntDecode(QtProtobufPrivate::Reader &reader);
    virtual void ntToJson(QJsonObject &object) const;
    virtual bool ntFromJson(const QJsonObject &object, QString *error);
    virtual QVariant ntProperty(QAnyStringView name) const;
    virtual bool ntSetProperty(QAnyStringView name, const QVariant &value);

private:
    friend struct QtProtobufPrivate::MessageAccess;
    QHash<qint32, QList<QByteArray>> m_unknown; // the raw fields, with their tags, that the class does not know
};
