// MIT License - see LICENSE
//
// The wire format of Nt sessions: the 16-byte header, the optional HMAC trailer and the CBOR keys of beacons and
// control messages. See doc/internal/twin-multiplayer-design.md. Not part of the public API.
#pragma once

#include "qcommssession.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qcbormap.h>

namespace QtComms {
namespace Protocol {

constexpr quint8 kVersion = 1;
constexpr qsizetype kHeaderSize = 16;
constexpr qsizetype kTrailerSize = 16;
constexpr qsizetype kMaxDatagram = 1200;
constexpr qsizetype kMaxFragment = 1100;
constexpr qsizetype kMaxMessage = 64 * 1024;

enum class Type : quint8 {
    Beacon = 1,
    Query = 2,
    Reliable = 3,
    Ack = 4,
    Heartbeat = 5,
    Ping = 6,
    Pong = 7,
    Refused = 8
};

enum Flag : quint16 { Authenticated = 0x1, More = 0x4, AckValid = 0x8 };

// Control messages, carried reliably. Key 0 of the CBOR map is the message type.
enum class Message : int {
    JoinRequest = 1,
    JoinAccept = 2,
    JoinReject = 3,
    PeerJoined = 4,
    PeerLeft = 5,
    Leave = 6,
    Application = 7,
    HostChanged = 8,
    SessionAttributes = 9
};

struct Header {
    Type type = Type::Beacon;
    quint16 flags = 0;
    quint32 session = 0;
    quint16 peer = 0;
    quint16 epoch = 0;
};

QByteArray encodeHeader(const Header &header);
bool isSessionPacket(const QByteArray &datagram);
bool decodeHeader(const QByteArray &datagram, Header *header);

void putU16(QByteArray *out, quint16 v);
void putU32(QByteArray *out, quint32 v);
void putI64(QByteArray *out, qint64 v);
quint16 getU16(const QByteArray &in, qsizetype at);
quint32 getU32(const QByteArray &in, qsizetype at);
qint64 getI64(const QByteArray &in, qsizetype at);

// The per-session key derived from a passphrase, and the 16-byte trailer computed with it.
QByteArray deriveKey(const QByteArray &passphrase, quint32 session);
QByteArray trailer(const QByteArray &key, const QByteArray &datagram);
// Appends the trailer if key is not empty.
QByteArray sign(const QByteArray &key, QByteArray datagram);
// Checks and strips the trailer. With an empty key, accepts anything.
bool verify(const QByteArray &key, QByteArray *datagram);

QCborMap encodeBeacon(const SessionInfo &info);
SessionInfo decodeBeacon(const QCborMap &map);
QCborMap encodePeer(const Peer &peer);
Peer decodePeer(const QCborMap &map);

QByteArray encodeCbor(const QCborMap &map);
bool decodeCbor(const QByteArray &bytes, QCborMap *map);

} // namespace Protocol
} // namespace QtComms
