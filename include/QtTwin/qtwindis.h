// MIT License - see LICENSE
#pragma once

#include "qtwinentity.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>

namespace QtTwin {

struct DisEntityId {
    quint16 site = 0;
    quint16 application = 0;
    quint16 entity = 0;

    constexpr DisEntityId() = default;
    constexpr DisEntityId(quint16 s, quint16 a, quint16 e) : site(s), application(a), entity(e) {}

    constexpr bool isValid() const { return site != 0 && application != 0 && entity != 0; }
    constexpr quint64 key() const {
        return (static_cast<quint64>(site) << 32) | (static_cast<quint64>(application) << 16) | entity;
    }
    static constexpr DisEntityId fromKey(quint64 key) {
        return DisEntityId(static_cast<quint16>(key >> 32), static_cast<quint16>(key >> 16), static_cast<quint16>(key));
    }
    constexpr bool operator==(const DisEntityId &o) const {
        return site == o.site && application == o.application && entity == o.entity;
    }
    constexpr bool operator!=(const DisEntityId &o) const { return !(*this == o); }
    QString toString() const;
};

enum class DisForce : quint8 { Other = 0, Friendly = 1, Opposing = 2, Neutral = 3 };

enum class DisPduType : quint8 { Other = 0, EntityState = 1, Fire = 2, Detonation = 3, Comment = 22 };

struct DisPduHeader {
    static constexpr qsizetype size = 12;

    quint8 version = 7;
    quint8 exercise = 1;
    DisPduType type = DisPduType::Other;
    quint8 family = 0;
    quint32 timestamp = 0;
    quint16 length = 0;
    quint8 status = 0;

    static bool peek(const QByteArray &data, qsizetype offset, DisPduHeader *header);
    static QList<qsizetype> split(const QByteArray &datagram);
};

struct DisEventId {
    quint16 site = 0;
    quint16 application = 0;
    quint16 number = 0;

    constexpr DisEventId() = default;
    constexpr DisEventId(quint16 s, quint16 a, quint16 n) : site(s), application(a), number(n) {}

    constexpr bool isValid() const { return site != 0 && application != 0 && number != 0; }
    constexpr quint64 key() const {
        return (static_cast<quint64>(site) << 32) | (static_cast<quint64>(application) << 16) | number;
    }
    constexpr bool operator==(const DisEventId &o) const {
        return site == o.site && application == o.application && number == o.number;
    }
    constexpr bool operator!=(const DisEventId &o) const { return !(*this == o); }
    QString toString() const;
};

enum class DisWarhead : quint16 {
    Other = 0,
    HighExplosive = 1000,
    HighExplosiveFragmentation = 1300,
    Kinetic = 4000,
    Practice = 8000
};

enum class DisFuse : quint16 { Other = 0, Contact = 100, Timed = 200, Proximity = 300, Command = 400 };

struct DisMunitionDescriptor {
    DisEntityType munition = DisEntityType(2, 1, 0, 0);
    DisWarhead warhead = DisWarhead::Kinetic;
    DisFuse fuse = DisFuse::Contact;
    quint16 quantity = 1;
    quint16 rate = 0;

    bool operator==(const DisMunitionDescriptor &o) const {
        return munition == o.munition && warhead == o.warhead && fuse == o.fuse && quantity == o.quantity &&
               rate == o.rate;
    }
    bool operator!=(const DisMunitionDescriptor &o) const { return !(*this == o); }
};

struct DisFirePdu {
    static constexpr quint8 pduType = 2;
    static constexpr qsizetype size = 96;

    quint8 exercise = 1;
    quint32 timestamp = 0;
    DisEntityId firingEntity;
    DisEntityId targetEntity;
    DisEntityId munitionEntity;
    DisEventId event;
    quint32 fireMissionIndex = 0;
    Vector3d location;
    DisMunitionDescriptor descriptor;
    Vector3d velocity;
    double range = 0.0;

    QByteArray encode() const;
    bool decode(const QByteArray &data, qsizetype offset = 0);
};

enum class DisDetonationResult : quint8 {
    Other = 0,
    EntityImpact = 1,
    EntityProximateDetonation = 2,
    GroundImpact = 3,
    GroundProximateDetonation = 4,
    Detonation = 5,
    None = 6,
    AirHit = 17,
    WaterImpact = 24,
    AirBurst = 25
};

struct DisDetonationPdu {
    static constexpr quint8 pduType = 3;
    static constexpr qsizetype baseSize = 104;

    quint8 exercise = 1;
    quint32 timestamp = 0;
    DisEntityId firingEntity;
    DisEntityId targetEntity;
    DisEntityId explodingEntity;
    DisEventId event;
    Vector3d velocity;
    Vector3d location;
    DisMunitionDescriptor descriptor;
    Vector3d locationInEntity;
    DisDetonationResult result = DisDetonationResult::Other;

    bool hitsTarget() const {
        return targetEntity.isValid() &&
               (result == DisDetonationResult::EntityImpact ||
                result == DisDetonationResult::EntityProximateDetonation || result == DisDetonationResult::AirHit);
    }
    QByteArray encode() const;
    bool decode(const QByteArray &data, qsizetype offset = 0);
};

struct DisEntityStatePdu {
    static constexpr quint8 protocolVersion = 7;
    static constexpr quint8 pduType = 1;
    static constexpr qsizetype baseSize = 144;

    quint8 exercise = 1;
    quint32 timestamp = 0;
    DisEntityId entityId;
    DisForce force = DisForce::Neutral;
    DisEntityType entityType;
    DisEntityType alternativeType;
    EntityState state;
    QByteArray marking;
    quint32 capabilities = 0;

    QByteArray encode() const;
    bool decode(const QByteArray &data, qsizetype offset = 0, qsizetype *length = nullptr);
    static QList<DisEntityStatePdu> decodeAll(const QByteArray &datagram);

    static quint32 timestampFromMsec(qint64 msec);
    static qint64 msecFromTimestamp(quint32 timestamp, qint64 reference);
};

} // namespace QtTwin
