// MIT License - see LICENSE
#pragma once

#include "qtwindis.h"
#include "qtwinphysics.h"

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

namespace QtTwin {

enum class DamageState : quint8 { None = 0, Slight = 1, Moderate = 2, Destroyed = 3 };

class DisAppearance {
public:
    enum class Smoke : quint8 { None = 0, Rising = 1, Emitting = 2, Both = 3 };

    constexpr DisAppearance() = default;
    constexpr explicit DisAppearance(quint32 bits) : m_bits(bits) {}

    constexpr quint32 bits() const { return m_bits; }
    constexpr DamageState damage() const { return static_cast<DamageState>((m_bits >> 3) & 3u); }
    constexpr DisAppearance withDamage(DamageState state) const {
        return DisAppearance((m_bits & ~(3u << 3)) | (static_cast<quint32>(state) << 3));
    }
    constexpr bool isDestroyed() const { return damage() == DamageState::Destroyed; }
    constexpr Smoke smoke() const { return static_cast<Smoke>((m_bits >> 5) & 3u); }
    constexpr DisAppearance withSmoke(Smoke smoke) const {
        return DisAppearance((m_bits & ~(3u << 5)) | (static_cast<quint32>(smoke) << 5));
    }
    constexpr bool isFlaming() const { return (m_bits & (1u << 15)) != 0; }
    constexpr DisAppearance withFlaming(bool on) const { return withBit(15, on); }
    constexpr bool isMobilityKilled() const { return (m_bits & (1u << 1)) != 0; }
    constexpr DisAppearance withMobilityKilled(bool on) const { return withBit(1, on); }
    constexpr bool isFirepowerKilled() const { return (m_bits & (1u << 2)) != 0; }
    constexpr DisAppearance withFirepowerKilled(bool on) const { return withBit(2, on); }
    constexpr bool isFrozen() const { return (m_bits & (1u << 21)) != 0; }
    constexpr DisAppearance withFrozen(bool on) const { return withBit(21, on); }
    constexpr bool isDeactivated() const { return (m_bits & (1u << 23)) != 0; }
    constexpr DisAppearance withDeactivated(bool on) const { return withBit(23, on); }
    constexpr DisAppearance destroyed() const {
        return withDamage(DamageState::Destroyed).withSmoke(Smoke::Both).withFlaming(true).withMobilityKilled(true)
            .withFirepowerKilled(true);
    }
    constexpr bool operator==(const DisAppearance &o) const { return m_bits == o.m_bits; }
    constexpr bool operator!=(const DisAppearance &o) const { return m_bits != o.m_bits; }

    static QString damageName(DamageState state);

private:
    constexpr DisAppearance withBit(int bit, bool on) const {
        return DisAppearance(on ? (m_bits | (1u << bit)) : (m_bits & ~(1u << bit)));
    }
    quint32 m_bits = 0;
};

class DamageModel {
public:
    DamageModel();

    int hitPoints(EntityKind kind) const;
    void setHitPoints(EntityKind kind, int points);
    int damageOf(const DisDetonationPdu &detonation) const;
    DamageState stateFor(EntityKind kind, int health) const;
    DamageState apply(EntityKind kind, int *health, int damage) const;

private:
    QHash<int, int> m_hitPoints;
};

struct Debris {
    Vector3d position;
    Vector3d velocity;
    Vector3d angularVelocity;
    double size = 0.1;
    double lifetime = 1.0;
};

struct DestructionEffect {
    Vector3d origin;
    double fireballRadius = 0.0;
    double fireballDuration = 0.0;
    double smokeDuration = 0.0;
    QList<Debris> debris;

    static DestructionEffect forEntity(const EntityState &entity, EntityKind kind, const BoundingBox &box,
                                       quint32 seed, int pieces = 24);
    void advance(const PhysicsEngine &physics, double dt);
};

struct Elimination {
    DisEntityId victim;
    QString victimMarking;
    quint16 victimOwner = 0;
    QString victimOwnerName;
    DisEntityId killer;
    QString killerMarking;
    quint16 killerOwner = 0;
    QString killerOwnerName;
    quint32 event = 0;
    qint64 time = 0;

    bool isValid() const { return victim.isValid(); }
    bool isSelfInflicted() const { return killer.isValid() && killer.application == victim.application; }
    QString toString() const;
};

} // namespace QtTwin
