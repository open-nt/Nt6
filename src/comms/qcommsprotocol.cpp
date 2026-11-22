// MIT License - see LICENSE
#include "qcommsprotocol_p.h"

#include <QtCore/qcborarray.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qcryptographichash.h>

namespace QtComms {
namespace Protocol {

namespace {

constexpr char kMagic[4] = {'N', 'T', 'S', 'S'};

enum BeaconKey : int {
    BId = 1,
    BEpoch,
    BName,
    BHostName,
    BProtocol,
    BMinProtocol,
    BApplication,
    BUpdateGroup,
    BUpdatePort,
    BExercise,
    BSite,
    BPeers,
    BMaxPeers,
    BFlags,
    BAttributes,
    BControlPort
};

enum PeerKey : int { PNumber = 1, PName, PRole, PAddress, PControlPort, PAttributes };

} // namespace

void putU16(QByteArray *out, quint16 v) {
    out->append(static_cast<char>(v & 0xff));
    out->append(static_cast<char>(v >> 8));
}

void putU32(QByteArray *out, quint32 v) {
    putU16(out, static_cast<quint16>(v & 0xffff));
    putU16(out, static_cast<quint16>(v >> 16));
}

void putI64(QByteArray *out, qint64 v) {
    const auto u = static_cast<quint64>(v);
    putU32(out, static_cast<quint32>(u & 0xffffffffu));
    putU32(out, static_cast<quint32>(u >> 32));
}

quint16 getU16(const QByteArray &in, qsizetype at) {
    const auto *d = reinterpret_cast<const uchar *>(in.constData()) + at;
    return static_cast<quint16>(d[0] | (d[1] << 8));
}

quint32 getU32(const QByteArray &in, qsizetype at) {
    return static_cast<quint32>(getU16(in, at)) | (static_cast<quint32>(getU16(in, at + 2)) << 16);
}

qint64 getI64(const QByteArray &in, qsizetype at) {
    const quint64 u = static_cast<quint64>(getU32(in, at)) | (static_cast<quint64>(getU32(in, at + 4)) << 32);
    return static_cast<qint64>(u);
}

QByteArray encodeHeader(const Header &header) {
    QByteArray out;
    out.reserve(kMaxDatagram);
    out.append(kMagic, 4);
    out.append(static_cast<char>(kVersion));
    out.append(static_cast<char>(header.type));
    putU16(&out, header.flags);
    putU32(&out, header.session);
    putU16(&out, header.peer);
    putU16(&out, header.epoch);
    return out;
}

bool isSessionPacket(const QByteArray &datagram) {
    return datagram.size() >= kHeaderSize && datagram.startsWith(QByteArray(kMagic, 4));
}

bool decodeHeader(const QByteArray &datagram, Header *header) {
    if (!isSessionPacket(datagram) || static_cast<quint8>(datagram[4]) != kVersion)
        return false;
    const auto type = static_cast<quint8>(datagram[5]);
    if (type < static_cast<quint8>(Type::Beacon) || type > static_cast<quint8>(Type::Refused))
        return false;
    header->type = static_cast<Type>(type);
    header->flags = getU16(datagram, 6);
    header->session = getU32(datagram, 8);
    header->peer = getU16(datagram, 12);
    header->epoch = getU16(datagram, 14);
    return true;
}

QByteArray deriveKey(const QByteArray &passphrase, quint32 session) {
    if (passphrase.isEmpty())
        return QByteArray();
    QByteArray label("nt-session");
    putU32(&label, session);
    return QMessageAuthenticationCode::hash(label, passphrase, QCryptographicHash::Sha256);
}

QByteArray trailer(const QByteArray &key, const QByteArray &datagram) {
    return QMessageAuthenticationCode::hash(datagram, key, QCryptographicHash::Sha256).left(kTrailerSize);
}

QByteArray sign(const QByteArray &key, QByteArray datagram) {
    if (!key.isEmpty())
        datagram.append(trailer(key, datagram));
    return datagram;
}

bool verify(const QByteArray &key, QByteArray *datagram) {
    if (key.isEmpty())
        return true;
    if (datagram->size() < kTrailerSize)
        return false;
    const QByteArray body = datagram->left(datagram->size() - kTrailerSize);
    const QByteArray expected = trailer(key, body);
    const QByteArray actual = datagram->right(kTrailerSize);
    // Compare without an early exit, so the time taken says nothing about the trailer.
    quint8 diff = 0;
    for (qsizetype i = 0; i < kTrailerSize; ++i)
        diff = static_cast<quint8>(diff | (static_cast<quint8>(expected[i]) ^ static_cast<quint8>(actual[i])));
    if (diff != 0)
        return false;
    *datagram = body;
    return true;
}

QCborMap encodeBeacon(const SessionInfo &info) {
    QCborMap m;
    m.insert(BId, static_cast<qint64>(info.id));
    m.insert(BEpoch, info.epoch);
    m.insert(BName, info.name);
    m.insert(BHostName, info.hostName);
    m.insert(BProtocol, info.protocolVersion);
    m.insert(BMinProtocol, info.minimumProtocolVersion);
    m.insert(BApplication, info.application);
    m.insert(BUpdateGroup, info.updateGroup.toString());
    m.insert(BUpdatePort, info.updatePort);
    m.insert(BExercise, info.exercise);
    m.insert(BSite, info.site);
    m.insert(BPeers, info.peerCount);
    m.insert(BMaxPeers, info.maximumPeers);
    m.insert(BFlags, (info.keyRequired ? 1 : 0) | (info.observersAllowed ? 2 : 0));
    m.insert(BAttributes, info.attributes);
    m.insert(BControlPort, info.controlPort);
    return m;
}

SessionInfo decodeBeacon(const QCborMap &m) {
    SessionInfo info;
    info.id = static_cast<quint32>(m.value(BId).toInteger());
    info.epoch = static_cast<quint16>(m.value(BEpoch).toInteger());
    info.name = m.value(BName).toString();
    info.hostName = m.value(BHostName).toString();
    info.protocolVersion = static_cast<int>(m.value(BProtocol).toInteger());
    info.minimumProtocolVersion = static_cast<int>(m.value(BMinProtocol).toInteger());
    info.application = m.value(BApplication).toString();
    info.updateGroup = QHostAddress(m.value(BUpdateGroup).toString());
    info.updatePort = static_cast<quint16>(m.value(BUpdatePort).toInteger());
    info.exercise = static_cast<quint8>(m.value(BExercise).toInteger(1));
    info.site = static_cast<quint16>(m.value(BSite).toInteger(1));
    info.peerCount = static_cast<int>(m.value(BPeers).toInteger());
    info.maximumPeers = static_cast<int>(m.value(BMaxPeers).toInteger());
    const qint64 flags = m.value(BFlags).toInteger();
    info.keyRequired = (flags & 1) != 0;
    info.observersAllowed = (flags & 2) != 0;
    info.attributes = m.value(BAttributes).toMap();
    info.controlPort = static_cast<quint16>(m.value(BControlPort).toInteger());
    if (!info.updateGroup.isMulticast() || info.updatePort == 0 || info.controlPort == 0)
        info.id = 0;
    return info;
}

QCborMap encodePeer(const Peer &peer) {
    QCborMap m;
    m.insert(PNumber, peer.number);
    m.insert(PName, peer.name);
    m.insert(PRole, static_cast<int>(peer.role));
    m.insert(PAddress, peer.address.toString());
    m.insert(PControlPort, peer.controlPort);
    m.insert(PAttributes, peer.attributes);
    return m;
}

Peer decodePeer(const QCborMap &m) {
    Peer p;
    p.number = static_cast<quint16>(m.value(PNumber).toInteger());
    p.name = m.value(PName).toString();
    p.role = m.value(PRole).toInteger() == 1 ? Peer::Role::Observer : Peer::Role::Participant;
    p.address = QHostAddress(m.value(PAddress).toString());
    p.controlPort = static_cast<quint16>(m.value(PControlPort).toInteger());
    p.attributes = m.value(PAttributes).toMap();
    return p;
}

QByteArray encodeCbor(const QCborMap &map) { return QCborValue(map).toCbor(); }

bool decodeCbor(const QByteArray &bytes, QCborMap *map) {
    QCborParserError error;
    const QCborValue v = QCborValue::fromCbor(bytes, &error);
    if (error.error != QCborError::NoError || !v.isMap())
        return false;
    *map = v.toMap();
    return true;
}

} // namespace Protocol
} // namespace QtComms
