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
