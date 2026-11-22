// MIT License - see LICENSE
#include "qcommschat.h"

#include "qcommsprotocol_p.h"

#include <QtCore/qcborvalue.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qhash.h>
#include <QtCore/qset.h>
#include <QtCore/quuid.h>
#include <QtCore/qxmlstream.h>
#include <QtNetwork/qnetworkdatagram.h>
#include <QtNetwork/qudpsocket.h>

#include <cmath>

namespace QtComms {

namespace {

constexpr qsizetype kMaxCot = 16 * 1024;
constexpr qsizetype kHistory = 500;
constexpr qsizetype kBacklog = 50;
constexpr qsizetype kSeen = 4096;
constexpr int kRateLimit = 5;
constexpr double kUnknownError = 9999999.0;

QString cotTime(const QDateTime &t) { return t.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ")); }

QDateTime parseCotTime(const QString &text) {
    QDateTime t = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!t.isValid())
        t = QDateTime::fromString(text, Qt::ISODate);
    return t.toUTC();
}

QString cleanText(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (const QChar c : text)
        out.append(c.unicode() < 0x20 || c.unicode() == 0x7f ? QLatin1Char(' ') : c);
    while (out.toUtf8().size() > Chat::maximumTextLength) {
        out.chop(1);
        if (!out.isEmpty() && out.back().isHighSurrogate())
            out.chop(1);
    }
    return out;
}

const char *cotType(ChatMessage::Kind kind) {
    switch (kind) {
    case ChatMessage::Kind::Message:
        return "b-t-f";
    case ChatMessage::Kind::Delivered:
        return "b-t-f-d";
    case ChatMessage::Kind::Read:
        return "b-t-f-r";
    }
    return "b-t-f";
}

} // namespace

class ChatPrivate {
public:
    Chat *q = nullptr;
    Session *session = nullptr;
    QString callsign;
    double latitude = 0.0;
    double longitude = 0.0;
    double height = 0.0;
    bool hasPosition = false;
    QStringList rooms;
    QList<ChatMessage> history;
    QSet<QString> seen;
    QList<QString> seenOrder;
    QHash<QString, QList<qint64>> recent;
    QHash<quint16, QString> names;
    QElapsedTimer clock;
    QUdpSocket *tak = nullptr;
    QHostAddress takGroup = QHostAddress(QStringLiteral("224.10.10.1"));
    quint16 takPort = 17012;
    bool warnedTakV1 = false;

    QString effectiveCallsign() const {
        if (!callsign.isEmpty())
            return callsign;
        const QString name = session->peer(session->localPeer()).name;
        return name.isEmpty() ? QStringLiteral("NT") : name;
    }
    bool markSeen(const QString &uid);
    bool rateLimited(const QString &sender);
    ChatMessage compose(const QString &text, const QString &room, const QString &roomId) const;
    QCborMap payload(const ChatMessage &m, bool backlog = false, bool fromTak = false) const;
    void deliver(const ChatMessage &m);
    void receive(quint16 peer, const QCborMap &message);
    void receiveTak();
    void sendToTak(const ChatMessage &m);
    void sendReceipt(const ChatMessage &m, ChatMessage::Kind kind);
    void postSystem(const QString &text);
    bool forMe(const ChatMessage &m) const {
        return m.system || m.roomId == ChatMessage::allChatRooms() || m.roomId == q->uid() || rooms.contains(m.roomId);
    }
};

bool ChatPrivate::markSeen(const QString &uid) {
    if (seen.contains(uid))
        return false;
    seen.insert(uid);
    seenOrder.append(uid);
    while (seenOrder.size() > kSeen)
        seen.remove(seenOrder.takeFirst());
    return true;
}

bool ChatPrivate::rateLimited(const QString &sender) {
    QList<qint64> &times = recent[sender];
    const qint64 now = clock.elapsed();
    while (!times.isEmpty() && now - times.first() >= 1000)
        times.removeFirst();
    if (times.size() >= kRateLimit)
        return true;
    times.append(now);
    return false;
}

ChatMessage ChatPrivate::compose(const QString &text, const QString &room, const QString &roomId) const {
    ChatMessage m;
    m.senderUid = q->uid();
    m.senderCallsign = effectiveCallsign();
    m.room = room;
    m.roomId = roomId;
    m.text = cleanText(text);
    m.time = QDateTime::currentDateTimeUtc();
    m.latitude = latitude;
    m.longitude = longitude;
    m.height = height;
    m.hasPosition = hasPosition;
    m.peer = session->localPeer();
    m.uid =
        QStringLiteral("GeoChat.%1.%2.%3").arg(m.senderUid, roomId, QUuid::createUuid().toString(QUuid::WithoutBraces));
    return m;
}

QCborMap ChatPrivate::payload(const ChatMessage &m, bool backlog, bool fromTak) const {
    QCborMap p;
    p.insert(1, QString::fromUtf8(m.toCot()));
    if (backlog)
        p.insert(2, true);
    if (fromTak)
        p.insert(3, true);
    return p;
}

void ChatPrivate::deliver(const ChatMessage &m) {
    history.append(m);
    while (history.size() > kHistory)
        history.removeFirst();
    emit q->messageReceived(m);
}

void ChatPrivate::sendToTak(const ChatMessage &m) {
    if (tak && m.roomId == ChatMessage::allChatRooms())
        tak->writeDatagram(m.toCot(), takGroup, takPort);
}

void ChatPrivate::sendReceipt(const ChatMessage &m, ChatMessage::Kind kind) {
    if (m.peer == 0 || m.peer == session->localPeer())
        return;
    ChatMessage r = compose(QString(), m.room, m.roomId);
    r.kind = kind;
    r.uid = m.uid;
    session->sendMessage(m.peer, Chat::channel, payload(r));
}

void ChatPrivate::receive(quint16 peer, const QCborMap &message) {
    bool ok = false;
    ChatMessage m = ChatMessage::fromCot(message.value(1).toString().toUtf8(), &ok);
    if (!ok)
        return;
    m.peer = peer;
    if (m.kind == ChatMessage::Kind::Delivered) {
        emit q->messageDelivered(m.uid, peer);
        return;
    }
    if (m.kind == ChatMessage::Kind::Read) {
        emit q->messageRead(m.uid, peer);
        return;
    }
    const bool backlog = message.value(2).toBool();
    m.fromTak = message.value(3).toBool();
    if (m.fromTak)
        m.peer = 0;
    if (!forMe(m) || !markSeen(m.uid) || (!backlog && rateLimited(m.senderUid)))
        return;
    deliver(m);
    if (!backlog && !m.system && !m.fromTak)
        sendReceipt(m, ChatMessage::Kind::Delivered);
    if (!m.fromTak)
        sendToTak(m);
}

void ChatPrivate::receiveTak() {
    while (tak && tak->hasPendingDatagrams()) {
        const QByteArray data = tak->receiveDatagram(kMaxCot + 1).data();
        if (data.startsWith("\xbf\x01\xbf")) {
            if (!warnedTakV1) {
                qWarning("QtComms::Chat: ignoring TAK protocol version 1 (protobuf) traffic; set the TAK device to "
                         "protocol version 0");
                warnedTakV1 = true;
            }
            continue;
        }
        bool ok = false;
        ChatMessage m = ChatMessage::fromCot(data, &ok);
        if (!ok || m.kind != ChatMessage::Kind::Message || m.senderUid.startsWith(QLatin1String("NT-")) ||
            !markSeen(m.uid) || rateLimited(m.senderUid))
            continue;
        m.fromTak = true;
        m.peer = 0;
        deliver(m);
        if (session->state() == Session::State::Hosting || session->state() == Session::State::Joined)
            session->broadcastMessage(Chat::channel, payload(m, false, true));
    }
}

void ChatPrivate::postSystem(const QString &text) {
    ChatMessage m = compose(text, ChatMessage::allChatRooms(), ChatMessage::allChatRooms());
    m.senderCallsign = ChatMessage::systemCallsign();
    m.system = true;
    markSeen(m.uid);
    deliver(m);
    session->broadcastMessage(Chat::channel, payload(m));
    sendToTak(m);
}

/*!
    \class QtComms::ChatMessage
    \inmodule NtComms
    \brief The ChatMessage struct is one text message, in Cursor on Target GeoChat form.

    Chat in an Nt session uses GeoChat, the open chat format of ATAK, WinTAK and TAK Server: each
    message is a Cursor on Target (CoT) event of type \c b-t-f, with the sender's position and the
    text in its detail. toCot() writes that XML and fromCot() reads it, so the same messages can be
    exchanged with TAK devices. Delivery and read receipts are the events \c b-t-f-d and \c b-t-f-r.

    \code
    <event version="2.0" uid="GeoChat.NT-1a2b-2.All Chat Rooms.5c1e…" type="b-t-f" how="h-g-i-g-o" …>
      <point lat="51.1789" lon="-1.8262" hae="160.0" ce="9999999" le="9999999"/>
      <detail>
        <__chat chatroom="All Chat Rooms" id="All Chat Rooms" senderCallsign="ALPHA-1" …>
          <chatgrp uid0="NT-1a2b-2" uid1="All Chat Rooms" id="All Chat Rooms"/>
        </__chat>
        <link uid="NT-1a2b-2" type="a-f-G-U-C" relation="p-p"/>
        <remarks source="Nt.NT-1a2b-2" to="All Chat Rooms" time="…">Crossing the bridge now</remarks>
      </detail>
    </event>
    \endcode

    \differences
    Qt has no chat; this struct is part of Nt Comms. Only TAK protocol version 0 (XML) is
    supported, not version 1 (protobuf).
    \enddifferences

    \sa QtComms::Chat
*/

/*!
    \enum QtComms::ChatMessage::Kind

    \value Message A text message (\c b-t-f).
    \value Delivered A receipt saying a message arrived (\c b-t-f-d).
    \value Read A receipt saying a message was read (\c b-t-f-r).
*/

/*!
    \variable QtComms::ChatMessage::kind
    Whether this is a message or a receipt.
*/

/*!
    \variable QtComms::ChatMessage::uid
    The CoT event uid, unique per message: \c GeoChat.<sender>.<room>.<UUID>. A receipt carries the
    uid of the message it confirms.
*/

/*!
    \variable QtComms::ChatMessage::senderUid
    The sender's uid. Nt peers are \c NT-<session id in hexadecimal>-<peer number>.
*/

/*!
    \variable QtComms::ChatMessage::senderCallsign
    The sender's callsign, as shown to users. System messages from the host use systemCallsign().
*/

/*!
    \variable QtComms::ChatMessage::room
    The name of the room, as shown to users: \c {All Chat Rooms}, a team name, or the recipient's
    name for a direct message.
*/

/*!
    \variable QtComms::ChatMessage::roomId
    The ID of the room: \c {All Chat Rooms}, a team name, or the recipient's uid for a direct
    message.
*/

/*!
    \variable QtComms::ChatMessage::text
    The text, at most Chat::maximumTextLength bytes of UTF-8, without control characters.
*/

/*!
    \variable QtComms::ChatMessage::time
    When the message was sent, in UTC.
*/

/*!
    \variable QtComms::ChatMessage::latitude
    The sender's latitude in degrees, if hasPosition.
*/

/*!
    \variable QtComms::ChatMessage::longitude
    The sender's longitude in degrees, if hasPosition.
*/

/*!
    \variable QtComms::ChatMessage::height
    The sender's height above the WGS84 ellipsoid in metres, if hasPosition.
*/

/*!
    \variable QtComms::ChatMessage::hasPosition
    Whether the sender's position is known.
*/

/*!
    \variable QtComms::ChatMessage::system
    Whether the host of the session wrote the message, such as a notice that a peer joined.
*/

/*!
    \variable QtComms::ChatMessage::fromTak
    Whether the message came from a TAK device through a TAK bridge.
*/

/*!
    \variable QtComms::ChatMessage::peer
    The peer the message came from, or 0 for a TAK device.
*/

/*!
    Returns the name of the room every participant is in: \c {All Chat Rooms}.
*/
QString ChatMessage::allChatRooms() { return QStringLiteral("All Chat Rooms"); }

/*!
    Returns the callsign of system messages: \c SESSION.
*/
QString ChatMessage::systemCallsign() { return QStringLiteral("SESSION"); }

/*!
    Returns the message as a Cursor on Target event, in XML.
*/
QByteArray ChatMessage::toCot() const {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("event"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("2.0"));
    w.writeAttribute(QStringLiteral("uid"), uid);
    w.writeAttribute(QStringLiteral("type"), QString::fromLatin1(cotType(kind)));
    w.writeAttribute(QStringLiteral("how"), QStringLiteral("h-g-i-g-o"));
    const QDateTime t = time.isValid() ? time : QDateTime::currentDateTimeUtc();
    w.writeAttribute(QStringLiteral("time"), cotTime(t));
    w.writeAttribute(QStringLiteral("start"), cotTime(t));
    w.writeAttribute(QStringLiteral("stale"), cotTime(t.addDays(1)));
    w.writeStartElement(QStringLiteral("point"));
    w.writeAttribute(QStringLiteral("lat"), QString::number(hasPosition ? latitude : 0.0, 'f', 7));
    w.writeAttribute(QStringLiteral("lon"), QString::number(hasPosition ? longitude : 0.0, 'f', 7));
    w.writeAttribute(QStringLiteral("hae"), QString::number(hasPosition ? height : 0.0, 'f', 1));
    w.writeAttribute(QStringLiteral("ce"), hasPosition ? QStringLiteral("10.0") : QStringLiteral("9999999"));
    w.writeAttribute(QStringLiteral("le"), hasPosition ? QStringLiteral("10.0") : QStringLiteral("9999999"));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("detail"));
    if (kind == Kind::Message) {
        w.writeStartElement(QStringLiteral("__chat"));
        w.writeAttribute(QStringLiteral("parent"), QStringLiteral("RootContactGroup"));
        w.writeAttribute(QStringLiteral("groupOwner"), QStringLiteral("false"));
        w.writeAttribute(QStringLiteral("chatroom"), room);
        w.writeAttribute(QStringLiteral("id"), roomId);
        w.writeAttribute(QStringLiteral("senderCallsign"), senderCallsign);
        w.writeStartElement(QStringLiteral("chatgrp"));
        w.writeAttribute(QStringLiteral("uid0"), senderUid);
        w.writeAttribute(QStringLiteral("uid1"), roomId);
        w.writeAttribute(QStringLiteral("id"), roomId);
        w.writeEndElement();
        w.writeEndElement();
    }
    w.writeStartElement(QStringLiteral("link"));
    w.writeAttribute(QStringLiteral("uid"), senderUid);
    w.writeAttribute(QStringLiteral("type"), QStringLiteral("a-f-G-U-C"));
    w.writeAttribute(QStringLiteral("relation"), QStringLiteral("p-p"));
    w.writeEndElement();
    if (kind == Kind::Message) {
        w.writeStartElement(QStringLiteral("remarks"));
        w.writeAttribute(QStringLiteral("source"), QStringLiteral("Nt.") + senderUid);
        w.writeAttribute(QStringLiteral("to"), roomId);
        w.writeAttribute(QStringLiteral("time"), cotTime(t));
        w.writeCharacters(text);
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

/*!
    Returns the message in the Cursor on Target event \a xml, and sets *\a ok, if \a ok is not null,
    to whether \a xml is a GeoChat message or receipt that has not gone stale. Events larger than
    16 KiB, or with a document type declaration, are refused. The text is cleaned of control
    characters and cut to Chat::maximumTextLength bytes.
*/
ChatMessage ChatMessage::fromCot(const QByteArray &xml, bool *ok) {
    ChatMessage m;
    bool good = false;
    if (ok)
        *ok = false;
    if (xml.size() > kMaxCot || xml.contains("<!DOCTYPE") || xml.contains("<!ENTITY"))
        return m;
    QXmlStreamReader r(xml);
    QDateTime stale;
    bool sawEvent = false;
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement)
            continue;
        const QXmlStreamAttributes a = r.attributes();
        const QStringView name = r.name();
        if (name == QLatin1String("event")) {
            sawEvent = true;
            m.uid = a.value(QStringLiteral("uid")).toString();
            const QStringView type = a.value(QStringLiteral("type"));
            if (type == QLatin1String("b-t-f"))
                m.kind = Kind::Message;
            else if (type == QLatin1String("b-t-f-d"))
                m.kind = Kind::Delivered;
            else if (type == QLatin1String("b-t-f-r"))
                m.kind = Kind::Read;
            else
                return m;
            m.time = parseCotTime(a.value(QStringLiteral("time")).toString());
            stale = parseCotTime(a.value(QStringLiteral("stale")).toString());
            good = !m.uid.isEmpty();
        } else if (name == QLatin1String("point")) {
            m.latitude = a.value(QStringLiteral("lat")).toDouble();
            m.longitude = a.value(QStringLiteral("lon")).toDouble();
            m.height = a.value(QStringLiteral("hae")).toDouble();
            bool ceOk = false;
            const double ce = a.value(QStringLiteral("ce")).toDouble(&ceOk);
            m.hasPosition = std::isfinite(m.latitude) && std::isfinite(m.longitude) && (!ceOk || ce < kUnknownError) &&
                            std::fabs(m.latitude) <= 90.0 && std::fabs(m.longitude) <= 180.0;
        } else if (name == QLatin1String("__chat")) {
            m.room = a.value(QStringLiteral("chatroom")).toString();
            m.roomId = a.value(QStringLiteral("id")).toString();
            m.senderCallsign = a.value(QStringLiteral("senderCallsign")).toString();
        } else if (name == QLatin1String("chatgrp")) {
            if (m.senderUid.isEmpty())
                m.senderUid = a.value(QStringLiteral("uid0")).toString();
        } else if (name == QLatin1String("link")) {
            if (m.senderUid.isEmpty())
                m.senderUid = a.value(QStringLiteral("uid")).toString();
        } else if (name == QLatin1String("remarks")) {
            if (m.roomId.isEmpty())
                m.roomId = a.value(QStringLiteral("to")).toString();
            m.text = cleanText(r.readElementText(QXmlStreamReader::SkipChildElements));
        }
    }
    if (r.hasError() || !sawEvent || !good)
        return m;
    if (m.room.isEmpty())
        m.room = m.roomId;
    if (m.kind == Kind::Message && (m.roomId.isEmpty() || m.senderUid.isEmpty()))
        return m;
    if (stale.isValid() && stale < QDateTime::currentDateTimeUtc())
        return m;
    m.system = m.senderCallsign == systemCallsign();
    if (ok)
        *ok = true;
    return m;
}

/*!
    \class QtComms::Chat
    \inmodule NtComms
    \brief The Chat class sends and receives text messages in a session.

    A Chat sits on a QtComms::Session. Messages go to every peer, to a team room, or to one peer, over
    the session's reliable channel, so they arrive once and in order, and they are signed with the
    session key if there is one. Each message is a Cursor on Target GeoChat event (see
    QtComms::ChatMessage) carrying the sender's position, so a map can show it where the sender is.

    \code
    QtComms::Chat chat(&session);
    chat.setCallsign(QStringLiteral("ALPHA-1"));
    connect(&chat, &QtComms::Chat::messageReceived, [](const QtComms::ChatMessage &m) {
        qDebug() << m.senderCallsign << m.text;
    });
    chat.send(QStringLiteral("Crossing the bridge now"));
    \endcode

    messageReceived() is also emitted for this peer's own messages, so a chat window can show
    everything from one signal. Each receiving peer answers with a delivery receipt; markRead()
    sends a read receipt.

    The host keeps the last 50 messages and sends them to each peer that joins, and it writes system
    messages (callsign \c SESSION) when peers join, leave or are refused, such as "bob could not
    join: models missing". Create the Chat before host() or join() so that nothing is missed.

    setTakBridgeEnabled() also connects the room \c {All Chat Rooms} to TAK devices on the network,
    through the TAK chat multicast group \c 224.10.10.1, port 17012.

    \differences
    Qt has no chat; this class is part of Nt Comms. Messages are not encrypted, and messages through
    the TAK bridge are not authenticated, because TAK multicast has no authentication. Only TAK
    protocol version 0 (XML) is understood.
    \enddifferences

    \sa QtComms::ChatMessage
*/

/*!
    \variable QtComms::Chat::channel
    The session channel chat uses: 16.
*/

/*!
    \variable QtComms::Chat::maximumTextLength
    The longest text of a message, in bytes of UTF-8: 1000.
*/

/*!
    Constructs a chat on \a session, with the given \a parent.
*/
Chat::Chat(Session *session, QObject *parent) : QObject(parent), d(std::make_unique<ChatPrivate>()) {
    d->q = this;
    d->session = session;
    d->clock.start();
    connect(session, &Session::messageReceived, this, [this](quint16 peer, int ch, const QCborMap &message) {
        if (ch == channel)
            d->receive(peer, message);
    });
    connect(session, &Session::peerJoined, this, [this](const Peer &peer) {
        d->names.insert(peer.number, peer.name);
        if (!d->session->isHost())
            return;
        // Send the newest kBacklog room messages, oldest first.
        QList<ChatMessage> backlog;
        for (qsizetype i = d->history.size() - 1; i >= 0 && backlog.size() < kBacklog; --i) {
            const ChatMessage &m = d->history.at(i);
            if (!m.system && m.roomId == ChatMessage::allChatRooms())
                backlog.prepend(m);
        }
        for (const ChatMessage &m : std::as_const(backlog))
            d->session->sendMessage(peer.number, channel, d->payload(m, true));
        d->postSystem(QStringLiteral("%1 joined").arg(peer.name));
    });
    connect(session, &Session::peerLeft, this, [this](quint16 number) {
        const QString name = d->names.take(number);
        if (d->session->isHost() && !name.isEmpty())
            d->postSystem(QStringLiteral("%1 left").arg(name));
    });
    connect(session, &Session::joinRefused, this,
            [this](const Peer &candidate, Session::RejectReason, const QString &text) {
                if (d->session->isHost())
                    d->postSystem(QStringLiteral("%1 could not join: %2")
                                      .arg(candidate.name.isEmpty() ? QStringLiteral("A peer") : candidate.name, text));
            });
}

/*!
    Destroys the chat.
*/
Chat::~Chat() = default;

/*!
    Returns the session the chat sits on.
*/
Session *Chat::session() const { return d->session; }

/*!
    Returns this peer's uid in GeoChat: \c NT-<session id in hexadecimal>-<peer number>.
*/
QString Chat::uid() const { return uidForPeer(d->session->localPeer()); }

/*!
    Returns the GeoChat uid of the peer numbered \a peer.
*/
QString Chat::uidForPeer(quint16 peer) const {
    return QStringLiteral("NT-%1-%2").arg(d->session->info().id, 0, 16).arg(peer);
}

/*!
    Sets the callsign shown with this peer's messages to \a callsign. By default it is the peer's
    name in the session.
*/
void Chat::setCallsign(const QString &callsign) { d->callsign = callsign; }

/*!
    Returns the callsign shown with this peer's messages.
*/
QString Chat::callsign() const { return d->effectiveCallsign(); }

/*!
    Sets the position sent with this peer's messages to \a latitude and \a longitude in degrees and
    \a height in metres above the WGS84 ellipsoid.
*/
void Chat::setPosition(double latitude, double longitude, double height) {
    d->latitude = latitude;
    d->longitude = longitude;
    d->height = height;
    d->hasPosition = true;
}

/*!
    Sets the team rooms this peer reads, besides \c {All Chat Rooms} and its direct messages, to
    \a rooms.
*/
void Chat::setRooms(const QStringList &rooms) { d->rooms = rooms; }

/*!
    Returns the team rooms this peer reads.
*/
QStringList Chat::rooms() const { return d->rooms; }

/*!
    Sends \a text to \a room, \c {All Chat Rooms} by default, and returns the message's uid. Returns
    an empty string if not in a session or \a text is empty.
*/
QString Chat::send(const QString &text, const QString &room) {
    const Session::State s = d->session->state();
    if ((s != Session::State::Hosting && s != Session::State::Joined) || text.trimmed().isEmpty())
        return QString();
    const ChatMessage m = d->compose(text, room, room);
    d->markSeen(m.uid);
    d->deliver(m);
    d->session->broadcastMessage(channel, d->payload(m));
    d->sendToTak(m);
    return m.uid;
}

/*!
    Sends \a text to the peer numbered \a peer only, and returns the message's uid. Returns an empty
    string if there is no such peer or \a text is empty.
*/
QString Chat::sendDirect(quint16 peer, const QString &text) {
    const Peer target = d->session->peer(peer);
    if (!target.isValid() || peer == d->session->localPeer() || text.trimmed().isEmpty())
        return QString();
    const ChatMessage m = d->compose(text, target.name, uidForPeer(peer));
    d->markSeen(m.uid);
    d->deliver(m);
    d->session->sendMessage(peer, channel, d->payload(m));
    return m.uid;
}

/*!
    Sends a read receipt for \a message to its sender.
*/
void Chat::markRead(const ChatMessage &message) { d->sendReceipt(message, ChatMessage::Kind::Read); }

/*!
    Returns the messages sent and received, oldest first: the last 500.
*/
QList<ChatMessage> Chat::history() const { return d->history; }

/*!
    Connects the room \c {All Chat Rooms} to TAK devices if \a enabled is \c true, or disconnects it.
    Messages sent and received in the session go to the TAK chat group as Cursor on Target XML, and
    messages from TAK devices are shown and passed on to the other peers, so one bridging peer is
    enough. Returns \c false if the TAK port cannot be opened.

    \sa setTakEndpoint()
*/
bool Chat::setTakBridgeEnabled(bool enabled) {
    if (d->tak) {
        d->tak->close();
        d->tak->deleteLater();
        d->tak = nullptr;
    }
    if (!enabled)
        return true;
    d->tak = new QUdpSocket(this);
    const SessionConfiguration config = d->session->configuration();
    if (!d->tak->bind(QHostAddress(QHostAddress::AnyIPv4), d->takPort,
                      QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint) ||
        !(config.networkInterface.isValid() ? d->tak->joinMulticastGroup(d->takGroup, config.networkInterface)
                                            : d->tak->joinMulticastGroup(d->takGroup))) {
        d->tak->deleteLater();
        d->tak = nullptr;
        return false;
    }
    d->tak->setSocketOption(QAbstractSocket::MulticastTtlOption, config.multicastTtl);
    d->tak->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    if (config.networkInterface.isValid())
        d->tak->setMulticastInterface(config.networkInterface);
    connect(d->tak, &QUdpSocket::readyRead, this, [this] { d->receiveTak(); });
    return true;
}

/*!
    Returns \c true if the TAK bridge is on.
*/
bool Chat::isTakBridgeEnabled() const { return d->tak != nullptr; }

/*!
    Sets the TAK chat multicast group to \a group and its port to \a port, \c 224.10.10.1 and 17012
    by default. It applies from the next setTakBridgeEnabled().
*/
void Chat::setTakEndpoint(const QHostAddress &group, quint16 port) {
    d->takGroup = group;
    d->takPort = port;
}

/*!
    \fn void QtComms::Chat::messageReceived(const QtComms::ChatMessage &message)
    This signal is emitted for each \a message sent or received, including this peer's own and the
    backlog a host sends on joining.
*/

/*!
    \fn void QtComms::Chat::messageDelivered(const QString &uid, quint16 peer)
    This signal is emitted when \a peer confirms that the message \a uid arrived.
*/

/*!
    \fn void QtComms::Chat::messageRead(const QString &uid, quint16 peer)
    This signal is emitted when \a peer marks the message \a uid as read.
*/

} // namespace QtComms

#include "moc_qcommschat.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
