// MIT License - see LICENSE
#include "qtwindis.h"

#include <algorithm>
#include <cstring>

namespace QtTwin {

namespace {

constexpr qint64 kHour = 3600000;
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
    void entityId(const DisEntityId &id) {
        u16(id.site);
        u16(id.application);
        u16(id.entity);
    }
    void eventId(const DisEventId &id) {
        u16(id.site);
        u16(id.application);
        u16(id.number);
    }
    void vector32(const Vector3d &v) {
        f32(v.x);
        f32(v.y);
        f32(v.z);
    }
    void vector64(const Vector3d &v) {
        f64(v.x);
        f64(v.y);
        f64(v.z);
    }
    void descriptor(const DisMunitionDescriptor &d) {
        type(d.munition);
        u16(static_cast<quint16>(d.warhead));
        u16(static_cast<quint16>(d.fuse));
        u16(d.quantity);
        u16(d.rate);
    }
    void header(quint8 exercise, quint8 pduType, quint8 family, quint32 timestamp, qsizetype length) {
        u8(DisEntityStatePdu::protocolVersion);
        u8(exercise);
        u8(pduType);
        u8(family);
        u32(timestamp);
        u16(static_cast<quint16>(length));
        u8(0); // PDU status
        u8(0); // padding
    }

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
    DisEntityId entityId() {
        DisEntityId id;
        id.site = u16();
        id.application = u16();
        id.entity = u16();
        return id;
    }
    DisEventId eventId() {
        DisEventId id;
        id.site = u16();
        id.application = u16();
        id.number = u16();
        return id;
    }
    Vector3d vector32() {
        Vector3d v;
        v.x = f32();
        v.y = f32();
        v.z = f32();
        return v;
    }
    Vector3d vector64() {
        Vector3d v;
        v.x = f64();
        v.y = f64();
        v.z = f64();
        return v;
    }
    DisMunitionDescriptor descriptor() {
        DisMunitionDescriptor d;
        d.munition = type();
        d.warhead = static_cast<DisWarhead>(u16());
        d.fuse = static_cast<DisFuse>(u16());
        d.quantity = u16();
        d.rate = u16();
        return d;
    }

private:
    const uchar *m_data;
    qsizetype m_pos;
};

// Reads the header of the PDU at offset and checks that it is a complete PDU of the given type.
bool readHeader(const QByteArray &data, qsizetype offset, quint8 pduType, qsizetype minimumSize, DisPduHeader *h) {
    if (!DisPduHeader::peek(data, offset, h))
        return false;
    return h->version >= 5 && h->version <= 7 && static_cast<quint8>(h->type) == pduType && h->length >= minimumSize &&
           h->length <= data.size() - offset;
}

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
    \enum QtTwin::DisPduType
    \inmodule NtTwin
    \brief The types of PDU Nt Twin knows.

    \value Other Any other PDU.
    \value EntityState An Entity State PDU, QtTwin::DisEntityStatePdu.
    \value Fire A Fire PDU, QtTwin::DisFirePdu.
    \value Detonation A Detonation PDU, QtTwin::DisDetonationPdu.
    \value Comment A Comment PDU.
*/

/*!
    \class QtTwin::DisPduHeader
    \inmodule NtTwin
    \brief The DisPduHeader struct is the 12-byte header every DIS PDU starts with.

    peek() reads the header of a PDU without decoding the rest, so a receiver can see what a
    datagram holds, and split() finds every PDU in a datagram that bundles several.

    \code
    for (qsizetype offset : QtTwin::DisPduHeader::split(datagram)) {
        QtTwin::DisPduHeader h;
        QtTwin::DisPduHeader::peek(datagram, offset, &h);
        if (h.type == QtTwin::DisPduType::Fire) { ... }
    }
    \endcode

    \differences
    Qt has no DIS support; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::DisPduHeader::size
    The size of the header: 12 bytes.
*/

/*!
    \variable QtTwin::DisPduHeader::version
    The protocol version, 7 for DIS 7.
*/

/*!
    \variable QtTwin::DisPduHeader::exercise
    The exercise ID.
*/

/*!
    \variable QtTwin::DisPduHeader::type
    The PDU type. Types Nt Twin does not know read as \c Other.
*/

/*!
    \variable QtTwin::DisPduHeader::family
    The protocol family: 1 for entity information, 2 for warfare.
*/

/*!
    \variable QtTwin::DisPduHeader::timestamp
    The DIS timestamp.
*/

/*!
    \variable QtTwin::DisPduHeader::length
    The length of the whole PDU in bytes.
*/

/*!
    \variable QtTwin::DisPduHeader::status
    The PDU status byte.
*/

/*!
    Reads the header at \a offset in \a data into *\a header and returns \c true, or returns
    \c false if fewer than 12 bytes are there.
*/
bool DisPduHeader::peek(const QByteArray &data, qsizetype offset, DisPduHeader *header) {
    if (offset < 0 || data.size() - offset < size)
        return false;
    Reader r(data, offset);
    DisPduHeader h;
    h.version = r.u8();
    h.exercise = r.u8();
    const quint8 type = r.u8();
    switch (type) {
    case 1:
    case 2:
    case 3:
    case 22:
        h.type = static_cast<DisPduType>(type);
        break;
    default:
        h.type = DisPduType::Other;
        break;
    }
    h.family = r.u8();
    h.timestamp = r.u32();
    h.length = r.u16();
    h.status = r.u8();
    if (header)
        *header = h;
    return true;
}

/*!
    Returns the offset of every PDU in \a datagram, which may bundle several back to back as DIS
    allows. Reading stops at the first PDU whose length is malformed.
*/
QList<qsizetype> DisPduHeader::split(const QByteArray &datagram) {
    QList<qsizetype> offsets;
    qsizetype offset = 0;
    while (datagram.size() - offset >= size) {
        const auto *d = reinterpret_cast<const uchar *>(datagram.constData()) + offset;
        const qsizetype pduLength = (static_cast<qsizetype>(d[8]) << 8) | d[9];
        if (pduLength < size || pduLength > datagram.size() - offset)
            break;
        offsets.append(offset);
        offset += pduLength;
    }
    return offsets;
}

/*!
    \class QtTwin::DisEventId
    \inmodule NtTwin
    \brief The DisEventId struct identifies an event, such as a shot, in a DIS exercise.

    The Fire PDU that starts a shot and the Detonation PDU that ends it carry the same event ID, so
    receivers can pair them. The site and application are those of the firing peer, and the number
    counts its shots.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::DisEventId::site
    The site number.
*/

/*!
    \variable QtTwin::DisEventId::application
    The application number within the site.
*/

/*!
    \variable QtTwin::DisEventId::number
    The event number within the application.
*/

/*!
    \fn constexpr QtTwin::DisEventId::DisEventId()
    Constructs the invalid event 0:0:0.
*/

/*!
    \fn constexpr QtTwin::DisEventId::DisEventId(quint16 s, quint16 a, quint16 n)
    Constructs the event with site \a s, application \a a and number \a n.
*/

/*!
    \fn constexpr bool QtTwin::DisEventId::isValid() const
    Returns \c true if no part is 0.
*/

/*!
    \fn constexpr quint64 QtTwin::DisEventId::key() const
    Returns the ID packed into one integer, for use as a key.
*/

/*!
    \fn constexpr bool QtTwin::DisEventId::operator==(const DisEventId &o) const
    Returns \c true if all three parts equal those of \a o.
*/

/*!
    \fn constexpr bool QtTwin::DisEventId::operator!=(const DisEventId &o) const
    Returns \c true if any part differs from \a o.
*/

/*!
    Returns the ID as \c site:application:number.
*/
QString DisEventId::toString() const { return QStringLiteral("%1:%2:%3").arg(site).arg(application).arg(number); }

/*!
    \enum QtTwin::DisWarhead
    \inmodule NtTwin
    \brief The warhead of a munition, from SISO-REF-010.

    \value Other Another warhead.
    \value HighExplosive A high-explosive warhead.
    \value HighExplosiveFragmentation A fragmenting high-explosive warhead.
    \value Kinetic A kinetic penetrator or a solid shot.
    \value Practice An inert practice round.
*/

/*!
    \enum QtTwin::DisFuse
    \inmodule NtTwin
    \brief The fuse of a munition, from SISO-REF-010.

    \value Other Another fuse.
    \value Contact Detonates on impact.
    \value Timed Detonates after a set time.
    \value Proximity Detonates near the target.
    \value Command Detonates on command.
*/

/*!
    \class QtTwin::DisMunitionDescriptor
    \inmodule NtTwin
    \brief The DisMunitionDescriptor struct says what was fired: the munition type, its warhead
    and fuse, and how much.

    The default is one anti-air kinetic round with a contact fuse, which is what a launcher that
    shoots drones fires.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DisFirePdu, QtTwin::DisDetonationPdu
*/

/*!
    \variable QtTwin::DisMunitionDescriptor::munition
    The munition's entity type, kind 2. The domain says what it is for: 1 anti-air, 2 anti-armour,
    6 anti-ship, 8 anti-personnel.
*/

/*!
    \variable QtTwin::DisMunitionDescriptor::warhead
    The warhead.
*/

/*!
    \variable QtTwin::DisMunitionDescriptor::fuse
    The fuse.
*/

/*!
    \variable QtTwin::DisMunitionDescriptor::quantity
    How many rounds the PDU stands for, 1 by default. A burst is one PDU with the number of rounds.
*/

/*!
    \variable QtTwin::DisMunitionDescriptor::rate
    The rate of fire of a burst in rounds per minute, 0 for a single shot.
*/

/*!
    \fn bool QtTwin::DisMunitionDescriptor::operator==(const DisMunitionDescriptor &o) const
    Returns \c true if every field equals that of \a o.
*/

/*!
    \fn bool QtTwin::DisMunitionDescriptor::operator!=(const DisMunitionDescriptor &o) const
    Returns \c true if any field differs from \a o.
*/

/*!
    \class QtTwin::DisFirePdu
    \inmodule NtTwin
    \brief The DisFirePdu struct is the Fire PDU of DIS 7: an entity has fired a munition.

    A peer sends a Fire PDU when one of its entities shoots: who fired, at what, from where, with
    which munition and at what velocity. Every peer that hears it can show the muzzle flash and fly
    the round with QtTwin::PhysicsEngine. The matching QtTwin::DisDetonationPdu, with the same event
    ID, says where the round ended up.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin. Only the munition form of the PDU is
    written; the expendable form (fire type indicator 1) decodes but is not distinguished.
    \enddifferences

    \sa QtTwin::DisDetonationPdu, QtTwin::DisEventId
*/

/*!
    \variable QtTwin::DisFirePdu::pduType
    The PDU type of a Fire PDU: 2.
*/

/*!
    \variable QtTwin::DisFirePdu::size
    The size of a Fire PDU: 96 bytes.
*/

/*!
    \variable QtTwin::DisFirePdu::exercise
    The exercise ID.
*/

/*!
    \variable QtTwin::DisFirePdu::timestamp
    The DIS timestamp of the shot.

    \sa QtTwin::DisEntityStatePdu::timestampFromMsec()
*/

/*!
    \variable QtTwin::DisFirePdu::firingEntity
    The entity that fired.
*/

/*!
    \variable QtTwin::DisFirePdu::targetEntity
    The entity fired at, or an invalid ID if there is no specific target.
*/

/*!
    \variable QtTwin::DisFirePdu::munitionEntity
    The entity ID of the round itself, if it is tracked as an entity; otherwise invalid.
*/

/*!
    \variable QtTwin::DisFirePdu::event
    The event, shared with the detonation that ends it.
*/

/*!
    \variable QtTwin::DisFirePdu::fireMissionIndex
    The fire mission the shot belongs to, 0 if none.
*/

/*!
    \variable QtTwin::DisFirePdu::location
    Where the round left the launcher, in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::DisFirePdu::descriptor
    What was fired.
*/

/*!
    \variable QtTwin::DisFirePdu::velocity
    The velocity of the round as it left, in metres per second.
*/

/*!
    \variable QtTwin::DisFirePdu::range
    The range to the target in metres as the shooter computed it, 0 if unknown.
*/

/*!
    Returns the PDU in DIS wire format.
*/
QByteArray DisFirePdu::encode() const {
    QByteArray out;
    out.reserve(size);
    Writer w(&out);
    w.header(exercise, pduType, 2, timestamp, size);
    w.entityId(firingEntity);
    w.entityId(targetEntity);
    w.entityId(munitionEntity);
    w.eventId(event);
    w.u32(fireMissionIndex);
    w.vector64(location);
    w.descriptor(descriptor);
    w.vector32(velocity);
    w.f32(range);
    return out;
}

/*!
    Reads the Fire PDU that starts at \a offset in \a data into this object. Returns \c false,
    leaving this object unchanged, if the bytes there are not a complete Fire PDU.
*/
bool DisFirePdu::decode(const QByteArray &data, qsizetype offset) {
    DisPduHeader h;
    if (!readHeader(data, offset, pduType, size, &h))
        return false;
    Reader r(data, offset + DisPduHeader::size);
    DisFirePdu p;
    p.exercise = h.exercise;
    p.timestamp = h.timestamp;
    p.firingEntity = r.entityId();
    p.targetEntity = r.entityId();
    p.munitionEntity = r.entityId();
    p.event = r.eventId();
    p.fireMissionIndex = r.u32();
    p.location = r.vector64();
    p.descriptor = r.descriptor();
    p.velocity = r.vector32();
    p.range = r.f32();
    *this = p;
    return true;
}

/*!
    \enum QtTwin::DisDetonationResult
    \inmodule NtTwin
    \brief What a munition hit when it detonated, from SISO-REF-010.

    \value Other Another result.
    \value EntityImpact The round struck the target entity.
    \value EntityProximateDetonation The round detonated close enough to the target to affect it.
    \value GroundImpact The round struck the ground.
    \value GroundProximateDetonation The round detonated near the ground.
    \value Detonation The round detonated with nothing in particular hit.
    \value None The round did not detonate: a dud.
    \value AirHit The round hit something in the air.
    \value WaterImpact The round struck the water.
    \value AirBurst The round burst in the air.
*/

/*!
    \class QtTwin::DisDetonationPdu
    \inmodule NtTwin
    \brief The DisDetonationPdu struct is the Detonation PDU of DIS 7: a munition has detonated or
    struck something.

    The peer that fired sends it when the round it flew ends: where, with what result and, when it
    hit an entity, where on that entity. The owner of the target then decides the damage, as DIS
    requires, shows it in the appearance of its Entity State PDUs (QtTwin::DisAppearance) and, if
    the entity is destroyed, reports the elimination.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin. Variable parameter records are skipped
    when decoding and none are written.
    \enddifferences

    \sa QtTwin::DisFirePdu, QtTwin::DamageModel
*/

/*!
    \variable QtTwin::DisDetonationPdu::pduType
    The PDU type of a Detonation PDU: 3.
*/

/*!
    \variable QtTwin::DisDetonationPdu::baseSize
    The size of a Detonation PDU without variable parameters: 104 bytes.
*/

/*!
    \variable QtTwin::DisDetonationPdu::exercise
    The exercise ID.
*/

/*!
    \variable QtTwin::DisDetonationPdu::timestamp
    The DIS timestamp of the detonation.
*/

/*!
    \variable QtTwin::DisDetonationPdu::firingEntity
    The entity that fired.
*/

/*!
    \variable QtTwin::DisDetonationPdu::targetEntity
    The entity hit, or an invalid ID if none.
*/

/*!
    \variable QtTwin::DisDetonationPdu::explodingEntity
    The entity ID of the round, if it was tracked as an entity; otherwise invalid.
*/

/*!
    \variable QtTwin::DisDetonationPdu::event
    The event of the shot, from its Fire PDU.
*/

/*!
    \variable QtTwin::DisDetonationPdu::velocity
    The velocity of the round at detonation, in metres per second.
*/

/*!
    \variable QtTwin::DisDetonationPdu::location
    Where it detonated, in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::DisDetonationPdu::descriptor
    What detonated.
*/

/*!
    \variable QtTwin::DisDetonationPdu::locationInEntity
    Where on the target it struck, in the target's body frame (forward, right, down), in metres.
    Zero unless an entity was hit.
*/

/*!
    \variable QtTwin::DisDetonationPdu::result
    What was hit.
*/

/*!
    \fn bool QtTwin::DisDetonationPdu::hitsTarget() const
    Returns \c true if the PDU names a target and the result says it was struck or affected.
*/

/*!
    Returns the PDU in DIS wire format.
*/
QByteArray DisDetonationPdu::encode() const {
    QByteArray out;
    out.reserve(baseSize);
    Writer w(&out);
    w.header(exercise, pduType, 2, timestamp, baseSize);
    w.entityId(firingEntity);
    w.entityId(targetEntity);
    w.entityId(explodingEntity);
    w.eventId(event);
    w.vector32(velocity);
    w.vector64(location);
    w.descriptor(descriptor);
    w.vector32(locationInEntity);
    w.u8(static_cast<quint8>(result));
    w.u8(0); // number of variable parameters
    w.u16(0);
    return out;
}

/*!
    Reads the Detonation PDU that starts at \a offset in \a data into this object. Returns
    \c false, leaving this object unchanged, if the bytes there are not a complete Detonation PDU.
*/
bool DisDetonationPdu::decode(const QByteArray &data, qsizetype offset) {
    DisPduHeader h;
    if (!readHeader(data, offset, pduType, baseSize, &h))
        return false;
    Reader r(data, offset + DisPduHeader::size);
    DisDetonationPdu p;
    p.exercise = h.exercise;
    p.timestamp = h.timestamp;
    p.firingEntity = r.entityId();
    p.targetEntity = r.entityId();
    p.explodingEntity = r.entityId();
    p.event = r.eventId();
    p.velocity = r.vector32();
    p.location = r.vector64();
    p.descriptor = r.descriptor();
    p.locationInEntity = r.vector32();
    p.result = static_cast<DisDetonationResult>(r.u8());
    const quint8 records = r.u8();
    if (baseSize + records * kRecordSize > h.length)
        return false;
    *this = p;
    return true;
}

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
    for (qsizetype offset : DisPduHeader::split(datagram)) {
        DisEntityStatePdu pdu;
        if (static_cast<uchar>(datagram.at(offset + 2)) == pduType && pdu.decode(datagram, offset))
            result.append(pdu);
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
