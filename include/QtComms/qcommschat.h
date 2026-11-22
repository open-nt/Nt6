// MIT License - see LICENSE
#pragma once

#include "qcommssession.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>

namespace QtComms {

struct ChatMessage {
    enum class Kind : quint8 { Message, Delivered, Read };

    Kind kind = Kind::Message;
    QString uid;
    QString senderUid;
    QString senderCallsign;
    QString room;
    QString roomId;
    QString text;
    QDateTime time;
    double latitude = 0.0;
    double longitude = 0.0;
    double height = 0.0;
    bool hasPosition = false;
    bool system = false;
    bool fromTak = false;
    quint16 peer = 0;

    static QString allChatRooms();
    static QString systemCallsign();
    QByteArray toCot() const;
    static ChatMessage fromCot(const QByteArray &xml, bool *ok = nullptr);
};

class ChatPrivate;

class Chat : public QObject {
    Q_OBJECT

public:
    static constexpr int channel = 16;
    static constexpr qsizetype maximumTextLength = 1000;

    explicit Chat(Session *session, QObject *parent = nullptr);
    ~Chat() override;

    Session *session() const;
    QString uid() const;
    QString uidForPeer(quint16 peer) const;
    void setCallsign(const QString &callsign);
    QString callsign() const;
    void setPosition(double latitude, double longitude, double height);
    void setRooms(const QStringList &rooms);
    QStringList rooms() const;

    QString send(const QString &text, const QString &room = ChatMessage::allChatRooms());
    QString sendDirect(quint16 peer, const QString &text);
    void markRead(const ChatMessage &message);
    QList<ChatMessage> history() const;

    bool setTakBridgeEnabled(bool enabled);
    bool isTakBridgeEnabled() const;
    void setTakEndpoint(const QHostAddress &group, quint16 port);

Q_SIGNALS:
    void messageReceived(const QtComms::ChatMessage &message);
    void messageDelivered(const QString &uid, quint16 peer);
    void messageRead(const QString &uid, quint16 peer);

private:
    std::unique_ptr<ChatPrivate> d;
};

} // namespace QtComms
