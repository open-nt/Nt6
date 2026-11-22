// MIT License - see LICENSE
#include "qtwindis.h"

#include <algorithm>
#include <cstring>

namespace QtTwin {

namespace {

constexpr qint64 kHour = 3600000;
constexpr qsizetype kHeaderSize = 12;
constexpr qsizetype kRecordSize = 16;
constexpr qsizetype kMarkingLength = 11;

class Writer {
public:
    explicit Writer(QByteArray *out) : m_out(out) {}
    void u8(quint8 v) { m_out->append(static_cast<char>(v)); }
    void u16(quint16 v) {
        u8(static_cast<quint8>(v >> 8));
        u8(static_cast<quint8>(v));
    }
    void u32(quint32 v) {
        u16(static_cast<quint16>(v >> 16));
        u16(static_cast<quint16>(v));
    }
    void u64(quint64 v) {
        u32(static_cast<quint32>(v >> 32));
        u32(static_cast<quint32>(v));
    }
    void f32(double v) {
        const float f = static_cast<float>(v);
        quint32 bits = 0;
        std::memcpy(&bits, &f, sizeof bits);
        u32(bits);
    }
    void f64(double v) {
        quint64 bits = 0;
        std::memcpy(&bits, &v, sizeof bits);
        u64(bits);
    }
    void type(const DisEntityType &t) {
        u8(t.kind);
        u8(t.domain);
        u16(t.country);
        u8(t.category);
        u8(t.subcategory);
        u8(t.specific);
        u8(t.extra);
    }
    void zeros(int n) { m_out->append(n, '\0'); }

private:
    QByteArray *m_out;
};

class Reader {
public:
    Reader(const QByteArray &data, qsizetype offset)
        : m_data(reinterpret_cast<const uchar *>(data.constData())), m_pos(offset) {}
    quint8 u8() { return m_data[m_pos++]; }
    quint16 u16() {
        const quint16 hi = u8();
        return static_cast<quint16>((hi << 8) | u8());
    }
    quint32 u32() {
        const quint32 hi = u16();
        return (hi << 16) | u16();
    }
    quint64 u64() {
        const quint64 hi = u32();
        return (hi << 32) | u32();
    }
    double f32() {
        const quint32 bits = u32();
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof f);
        return static_cast<double>(f);
    }
    double f64() {
        const quint64 bits = u64();
        double d = 0.0;
        std::memcpy(&d, &bits, sizeof d);
        return d;
    }
    DisEntityType type() {
        DisEntityType t;
        t.kind = u8();
        t.domain = u8();
        t.country = u16();
        t.category = u8();
        t.subcategory = u8();
        t.specific = u8();
        t.extra = u8();
        return t;
    }
    void skip(qsizetype n) { m_pos += n; }
    qsizetype position() const { return m_pos; }

private:
    const uchar *m_data;
    qsizetype m_pos;
};

} // namespace

/*!
    \class QtTwin::DisEntityId
    \inmodule NtTwin
    \brief The DisEntityId struct identifies an entity in a DIS exercise.

    An entity ID has three parts: the site, the application at that site, and the entity within the
    application. In an Nt session the site is the exercise's site number, the application is the
    owner's peer number, and the entity is the owner's own number for it.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::DisEntityId::site
    The site number.
*/

/*!
    \variable QtTwin::DisEntityId::application
    The application number within the site.
*/

/*!
    \variable QtTwin::DisEntityId::entity
    The entity number within the application.
*/

/*!
    \fn constexpr QtTwin::DisEntityId::DisEntityId()
    Constructs the invalid ID 0:0:0.
*/

/*!
    \fn constexpr QtTwin::DisEntityId::DisEntityId(quint16 s, quint16 a, quint16 e)
    Constructs the ID with site \a s, application \a a and entity \a e.
*/

/*!
    \fn constexpr bool QtTwin::DisEntityId::isValid() const
    Returns \c true if no part is 0.
*/

/*!
    \fn constexpr quint64 QtTwin::DisEntityId::key() const
    Returns the ID packed into one integer, for use as a key in a hash or map.

    \sa fromKey()
*/

/*!
    \fn static constexpr QtTwin::DisEntityId QtTwin::DisEntityId::fromKey(quint64 key)
    Returns the ID that key() packed into \a key.
*/

/*!
    \fn constexpr bool QtTwin::DisEntityId::operator==(const DisEntityId &o) const
    Returns \c true if all three parts equal those of \a o.
*/

/*!
    \fn constexpr bool QtTwin::DisEntityId::operator!=(const DisEntityId &o) const
    Returns \c true if any part differs from \a o.
*/

/*!
    Returns the ID as \c site:application:entity.
*/
QString DisEntityId::toString() const { return QStringLiteral("%1:%2:%3").arg(site).arg(application).arg(entity); }

/*!
    \enum QtTwin::DisForce
    \inmodule NtTwin
    \brief The side an entity is on in a DIS exercise.

    \value Other No side, or a side not listed.
    \value Friendly The friendly force.
    \value Opposing The opposing force.
    \value Neutral Neither side.
*/

/*!
    \class QtTwin::DisEntityStatePdu
    \inmodule NtTwin
    \brief The DisEntityStatePdu struct is the Entity State PDU of DIS 7 (IEEE 1278.1-2012).

    The Entity State PDU tells every participant where an entity is, how it moves and how it looks.
    encode() writes the 144 bytes of the PDU plus 16 bytes per articulated part, in network byte
    order, so Wireshark and other DIS tools can read it; decode() reads one PDU.

    \code
    QtTwin::DisEntityStatePdu pdu;
    pdu.exercise = 1;
    pdu.entityId = QtTwin::DisEntityId(1, myPeer, 1);
    pdu.entityType = model.disType;
    pdu.state = state;
    pdu.timestamp = QtTwin::DisEntityStatePdu::timestampFromMsec(state.time);
    pdu.marking = "ALPHA-1";
    socket.writeDatagram(pdu.encode(), group, port);
    \endcode

    The DIS timestamp only counts time within the current hour. msecFromTimestamp() turns it back
    into session time, given a reference close to the time it was sent.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin. Only the Entity State PDU is provided.
    Variable parameter records other than articulated parts are skipped when decoding, the
    dead-reckoning "other parameters" are written as zeros, and the marking is always ASCII.
    \enddifferences

    \sa QtTwin::DeadReckoning
*/

/*!
    \variable QtTwin::DisEntityStatePdu::protocolVersion
    The DIS protocol version written by encode(): 7.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::pduType
    The PDU type of an Entity State PDU: 1.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::baseSize
    The size in bytes of a PDU without articulated parts: 144.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::exercise
    The exercise ID, 1 to 255. Receivers ignore PDUs from other exercises.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::timestamp
    The DIS timestamp.

    \sa timestampFromMsec()
*/

/*!
    \variable QtTwin::DisEntityStatePdu::entityId
    The entity the PDU describes.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::force
    The side of the entity.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::entityType
    What the entity is.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::alternativeType
    What the entity looks like to the other side, normally the same as entityType.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::state
    The position, orientation, motion, appearance and articulated parts. Its time is not sent;
    timestamp is.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::marking
    The marking, such as a callsign: up to 11 ASCII characters.
*/

/*!
    \variable QtTwin::DisEntityStatePdu::capabilities
    The capability bits of the entity.
*/

/*!
    Returns the PDU in DIS wire format.
*/
QByteArray DisEntityStatePdu::encode() const {
    const qsizetype parts = std::min<qsizetype>(state.articulatedParts.size(), 255);
    const qsizetype length = baseSize + parts * kRecordSize;
    QByteArray out;
    out.reserve(length);
    Writer w(&out);
    w.u8(protocolVersion);
    w.u8(exercise);
    w.u8(pduType);
    w.u8(1); // protocol family: entity information
    w.u32(timestamp);
    w.u16(static_cast<quint16>(length));
    w.u8(0); // PDU status
    w.u8(0); // padding
    w.u16(entityId.site);
    w.u16(entityId.application);
    w.u16(entityId.entity);
    w.u8(static_cast<quint8>(force));
    w.u8(static_cast<quint8>(parts));
    w.type(entityType);
    w.type(alternativeType);
    w.f32(state.velocity.x);
    w.f32(state.velocity.y);
    w.f32(state.velocity.z);
    w.f64(state.position.x);
    w.f64(state.position.y);
    w.f64(state.position.z);
    w.f32(state.orientation.psi);
    w.f32(state.orientation.theta);
    w.f32(state.orientation.phi);
    w.u32(state.appearance);
    w.u8(static_cast<quint8>(state.deadReckoning));
    w.zeros(15); // other dead-reckoning parameters
    w.f32(state.acceleration.x);
    w.f32(state.acceleration.y);
    w.f32(state.acceleration.z);
    w.f32(state.angularVelocity.x);
    w.f32(state.angularVelocity.y);
    w.f32(state.angularVelocity.z);
    w.u8(1); // character set: ASCII
    const QByteArray mark = marking.left(kMarkingLength);
    out.append(mark);
    w.zeros(static_cast<int>(kMarkingLength - mark.size()));
    w.u32(capabilities);
    for (qsizetype i = 0; i < parts; ++i) {
        const ArticulatedPart &p = state.articulatedParts.at(i);
        w.u8(0); // record type: articulated part
        w.u8(p.changeIndicator);
        w.u16(p.attachedTo);
        w.u32(p.parameterType);
        w.f32(static_cast<double>(p.value));
        w.zeros(4);
    }
    return out;
}

/*!
    Reads the Entity State PDU that starts at \a offset in \a data into this object, and sets
    *\a length to its length, if \a length is not null. Returns \c false, leaving this object
    unchanged, if the bytes there are not a complete Entity State PDU of DIS version 5 to 7.
    state.time is set to 0.
*/
bool DisEntityStatePdu::decode(const QByteArray &data, qsizetype offset, qsizetype *length) {
    const qsizetype available = data.size() - offset;
    if (offset < 0 || available < baseSize)
        return false;
    Reader r(data, offset);
    const quint8 version = r.u8();
    const quint8 exerciseId = r.u8();
    const quint8 type = r.u8();
    r.skip(1);
    const quint32 stamp = r.u32();
    const quint16 pduLength = r.u16();
    r.skip(2);
    if (version < 5 || version > 7 || type != pduType || pduLength < baseSize || pduLength > available)
        return false;
    DisEntityStatePdu p;
    p.exercise = exerciseId;
    p.timestamp = stamp;
    p.entityId.site = r.u16();
    p.entityId.application = r.u16();
    p.entityId.entity = r.u16();
    p.force = static_cast<DisForce>(r.u8());
    const quint8 records = r.u8();
    if (baseSize + records * kRecordSize > pduLength)
        return false;
    p.entityType = r.type();
    p.alternativeType = r.type();
    p.state.velocity.x = r.f32();
    p.state.velocity.y = r.f32();
    p.state.velocity.z = r.f32();
    p.state.position.x = r.f64();
    p.state.position.y = r.f64();
    p.state.position.z = r.f64();
    p.state.orientation.psi = r.f32();
    p.state.orientation.theta = r.f32();
    p.state.orientation.phi = r.f32();
    p.state.appearance = r.u32();
    p.state.deadReckoning = static_cast<DeadReckoningAlgorithm>(r.u8());
    if (static_cast<quint8>(p.state.deadReckoning) > 9)
        p.state.deadReckoning = DeadReckoningAlgorithm::Other;
    r.skip(15);
    p.state.acceleration.x = r.f32();
    p.state.acceleration.y = r.f32();
    p.state.acceleration.z = r.f32();
    p.state.angularVelocity.x = r.f32();
    p.state.angularVelocity.y = r.f32();
    p.state.angularVelocity.z = r.f32();
    r.skip(1); // character set
    const qsizetype markingStart = r.position();
    QByteArray mark = data.mid(markingStart, kMarkingLength);
    const qsizetype nul = mark.indexOf('\0');
    if (nul >= 0)
        mark.truncate(nul);
    p.marking = mark;
    r.skip(kMarkingLength);
    p.capabilities = r.u32();
    for (int i = 0; i < records; ++i) {
        const quint8 recordType = r.u8();
        if (recordType != 0) {
            r.skip(kRecordSize - 1);
            continue;
        }
        ArticulatedPart part;
        part.changeIndicator = r.u8();
        part.attachedTo = r.u16();
        part.parameterType = r.u32();
        part.value = static_cast<float>(r.f32());
        r.skip(4);
        p.state.articulatedParts.append(part);
    }
    *this = p;
    if (length)
        *length = pduLength;
    return true;
}

/*!
    Returns the Entity State PDUs in \a datagram. DIS allows several PDUs back to back in one
    datagram; PDUs of other types are skipped, and reading stops at the first malformed one.
*/
QList<DisEntityStatePdu> DisEntityStatePdu::decodeAll(const QByteArray &datagram) {
    QList<DisEntityStatePdu> result;
    qsizetype offset = 0;
    while (datagram.size() - offset >= kHeaderSize) {
        const auto *d = reinterpret_cast<const uchar *>(datagram.constData()) + offset;
        const qsizetype pduLength = (static_cast<qsizetype>(d[8]) << 8) | d[9];
        if (pduLength < kHeaderSize || pduLength > datagram.size() - offset)
            break;
        if (d[2] == pduType) {
            DisEntityStatePdu pdu;
            if (pdu.decode(datagram, offset))
                result.append(pdu);
        }
        offset += pduLength;
    }
    return result;
}

/*!
    Returns the relative DIS timestamp for the session time \a msec: the time within the current
    hour in units of 3600/2^31 seconds, shifted left one bit, with the lowest bit 0.
*/
quint32 DisEntityStatePdu::timestampFromMsec(qint64 msec) {
    const qint64 withinHour = ((msec % kHour) + kHour) % kHour;
    const quint64 ticks = (static_cast<quint64>(withinHour) << 31) / static_cast<quint64>(kHour);
    return static_cast<quint32>(ticks << 1);
}

/*!
    Returns the session time in milliseconds of the DIS \a timestamp, taking the hour that puts it
    closest to \a reference. A receiver passes its own session time, which is close to the time the
    PDU was sent.
*/
qint64 DisEntityStatePdu::msecFromTimestamp(quint32 timestamp, qint64 reference) {
    const quint64 ticks = timestamp >> 1;
    const qint64 withinHour = static_cast<qint64>((ticks * static_cast<quint64>(kHour) + (1ull << 30)) >> 31);
    const qint64 base = reference - (((reference % kHour) + kHour) % kHour);
    qint64 t = base + withinHour;
    if (t - reference > kHour / 2)
        t -= kHour;
    else if (reference - t > kHour / 2)
        t += kHour;
    return t;
}

} // namespace QtTwin
