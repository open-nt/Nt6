// MIT License - see LICENSE
#include "qtwinkinetic.h"

#include <QtCore/qrandom.h>

#include <algorithm>
#include <cmath>

namespace QtTwin {

/*!
    \enum QtTwin::DamageState
    \inmodule NtTwin
    \brief How badly an entity is damaged, as the DIS appearance carries it.

    \value None Undamaged.
    \value Slight Slightly damaged; it still works.
    \value Moderate Moderately damaged.
    \value Destroyed Destroyed. The entity is eliminated and shown as a wreck.
*/

/*!
    \class QtTwin::DisAppearance
    \inmodule NtTwin
    \brief The DisAppearance class reads and writes the appearance bits of an Entity State PDU.

    The 32 appearance bits of QtTwin::EntityState::appearance say how an entity looks: its damage,
    smoke, flames, whether it can still move or shoot, and whether it is deactivated. The layout is
    the one DIS platforms of every domain share, and the health of a life form sits in the same bits
    as damage. DisAppearance is a value: each \c with function returns a new appearance.

    \code
    QtTwin::DisAppearance look(state.appearance);
    if (look.isDestroyed())
        drawWreck();
    state.appearance = look.withDamage(QtTwin::DamageState::Moderate).withSmoke(QtTwin::DisAppearance::Smoke::Emitting).bits();
    \endcode

    \differences
    Qt has no DIS support; this class is part of Nt Twin. Only the bits common to all domains are
    provided; lights, hatches and camouflage are not.
    \enddifferences

    \sa QtTwin::DamageModel, QtTwin::EntityState::appearance
*/

/*!
    \enum QtTwin::DisAppearance::Smoke
    The smoke an entity gives off.

    \value None No smoke.
    \value Rising A rising plume.
    \value Emitting Smoke emitted by the engine or a smoke generator.
    \value Both Both.
*/

/*!
    \fn constexpr QtTwin::DisAppearance::DisAppearance()
    Constructs the appearance of an undamaged entity with nothing set.
*/

/*!
    \fn constexpr QtTwin::DisAppearance::DisAppearance(quint32 bits)
    Constructs the appearance from the \a bits of an Entity State PDU.
*/

/*!
    \fn constexpr quint32 QtTwin::DisAppearance::bits() const
    Returns the bits, for QtTwin::EntityState::appearance.
*/

/*!
    \fn constexpr QtTwin::DamageState QtTwin::DisAppearance::damage() const
    Returns the damage, bits 3 and 4.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withDamage(DamageState state) const
    Returns this appearance with the damage set to \a state.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isDestroyed() const
    Returns \c true if the damage is \c Destroyed.
*/

/*!
    \fn constexpr QtTwin::DisAppearance::Smoke QtTwin::DisAppearance::smoke() const
    Returns the smoke, bits 5 and 6.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withSmoke(Smoke smoke) const
    Returns this appearance with the smoke set to \a smoke.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isFlaming() const
    Returns \c true if the entity is on fire, bit 15.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withFlaming(bool on) const
    Returns this appearance with the flames \a on or off.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isMobilityKilled() const
    Returns \c true if the entity can no longer move, bit 1.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withMobilityKilled(bool on) const
    Returns this appearance with the mobility kill \a on or off.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isFirepowerKilled() const
    Returns \c true if the entity can no longer shoot, bit 2.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withFirepowerKilled(bool on) const
    Returns this appearance with the firepower kill \a on or off.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isFrozen() const
    Returns \c true if the entity's simulation is frozen, bit 21.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withFrozen(bool on) const
    Returns this appearance with the frozen status \a on or off.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::isDeactivated() const
    Returns \c true if the entity is deactivated and should be removed from the exercise, bit 23.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::withDeactivated(bool on) const
    Returns this appearance with the deactivated state \a on or off.
*/

/*!
    \fn constexpr QtTwin::DisAppearance QtTwin::DisAppearance::destroyed() const
    Returns this appearance as a wreck: destroyed, smoking, burning, and unable to move or shoot.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::operator==(const DisAppearance &o) const
    Returns \c true if the bits equal those of \a o.
*/

/*!
    \fn constexpr bool QtTwin::DisAppearance::operator!=(const DisAppearance &o) const
    Returns \c true if the bits differ from \a o.
*/

/*!
    Returns \a state as text: "undamaged", "slight damage", "moderate damage" or "destroyed".
*/
QString DisAppearance::damageName(DamageState state) {
    switch (state) {
    case DamageState::None:
        return QStringLiteral("undamaged");
    case DamageState::Slight:
        return QStringLiteral("slight damage");
    case DamageState::Moderate:
        return QStringLiteral("moderate damage");
    case DamageState::Destroyed:
        return QStringLiteral("destroyed");
    }
    return QString();
}

/*!
    \class QtTwin::DamageModel
    \inmodule NtTwin
    \brief The DamageModel class decides what a hit does to an entity.

    Each kind of entity has a number of hit points: a drone or a person is destroyed by one hit, an
    aircraft by two, a ground vehicle by three and a boat by four. A detonation that strikes the
    target takes one point per round in its descriptor. The damage state follows from the points
    left: destroyed at none, moderate at a third or less, slight below full.

    The owner of an entity applies the model when a QtTwin::DisDetonationPdu names its entity, as
    DIS leaves the damage assessment to the owner:

    \code
    int health = model.hitPoints(kind);
    // on a detonation that hits:
    const QtTwin::DamageState state = model.apply(kind, &health, model.damageOf(detonation));
    entityState.appearance = QtTwin::DisAppearance(entityState.appearance).withDamage(state).bits();
    \endcode

    \differences
    Qt has no such class; this class is part of Nt Twin. There is no armour, no hit location and no
    warhead effect beyond the quantity.
    \enddifferences

    \sa QtTwin::DisAppearance, QtTwin::DestructionEffect
*/

/*!
    Constructs the model with the default hit points.
*/
DamageModel::DamageModel() {
    m_hitPoints.insert(static_cast<int>(EntityKind::Person), 1);
    m_hitPoints.insert(static_cast<int>(EntityKind::GroundVehicle), 3);
    m_hitPoints.insert(static_cast<int>(EntityKind::Aircraft), 2);
    m_hitPoints.insert(static_cast<int>(EntityKind::Drone), 1);
    m_hitPoints.insert(static_cast<int>(EntityKind::Boat), 4);
}

/*!
    Returns the hit points an undamaged entity of \a kind has.
*/
int DamageModel::hitPoints(EntityKind kind) const { return m_hitPoints.value(static_cast<int>(kind), 1); }

/*!
    Sets the hit points of entities of \a kind to \a points, at least 1.
*/
void DamageModel::setHitPoints(EntityKind kind, int points) {
    m_hitPoints.insert(static_cast<int>(kind), std::max(1, points));
}

/*!
    Returns the hit points \a detonation takes from its target: one per round if it struck or
    affected the target, otherwise 0.
*/
int DamageModel::damageOf(const DisDetonationPdu &detonation) const {
    if (!detonation.hitsTarget())
        return 0;
    return std::max<int>(1, detonation.descriptor.quantity);
}

/*!
    Returns the damage state of an entity of \a kind with \a health hit points left.
*/
DamageState DamageModel::stateFor(EntityKind kind, int health) const {
    const int full = hitPoints(kind);
    if (health <= 0)
        return DamageState::Destroyed;
    if (health >= full)
        return DamageState::None;
    if (health * 3 <= full)
        return DamageState::Moderate;
    return DamageState::Slight;
}

/*!
    Takes \a damage hit points from *\a health, not below 0, and returns the resulting damage
    state of an entity of \a kind.
*/
DamageState DamageModel::apply(EntityKind kind, int *health, int damage) const {
    *health = std::max(0, *health - std::max(0, damage));
    return stateFor(kind, *health);
}

/*!
    \class QtTwin::Debris
    \inmodule NtTwin
    \brief The Debris struct is one piece thrown off a destroyed entity.

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DestructionEffect
*/

/*!
    \variable QtTwin::Debris::position
    The position in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::Debris::velocity
    The velocity in metres per second.
*/

/*!
    \variable QtTwin::Debris::angularVelocity
    The spin in radians per second about each body axis.
*/

/*!
    \variable QtTwin::Debris::size
    The size of the piece in metres.
*/

/*!
    \variable QtTwin::Debris::lifetime
    How long the piece is shown, in seconds.
*/

/*!
    \class QtTwin::DestructionEffect
    \inmodule NtTwin
    \brief The DestructionEffect struct describes how a destroyed entity comes apart, so that every
    peer shows the same explosion.

    When a vehicle, boat, aircraft or drone is destroyed, the viewer swaps its model for the wreck
    in the catalogue (QtTwin::ModelRef::wreck) or darkens it, and plays this effect: a fireball,
    smoke and debris flung from the entity. forEntity() builds it from a seed, so peers that use the
    event number of the shot get the same pieces without sending them, and advance() flies the
    debris under gravity and drag with a QtTwin::PhysicsEngine.

    \code
    QtTwin::DestructionEffect boom = QtTwin::DestructionEffect::forEntity(state, kind, box, detonation.event.number);
    // each frame:
    boom.advance(physics, dt);
    for (const QtTwin::Debris &piece : boom.debris)
        drawCube(piece.position, piece.size);
    \endcode

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DamageModel, QtTwin::PhysicsEngine
*/

/*!
    \variable QtTwin::DestructionEffect::origin
    The centre of the explosion in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::DestructionEffect::fireballRadius
    The radius of the fireball in metres.
*/

/*!
    \variable QtTwin::DestructionEffect::fireballDuration
    How long the fireball lasts, in seconds.
*/

/*!
    \variable QtTwin::DestructionEffect::smokeDuration
    How long the wreck smokes, in seconds.
*/

/*!
    \variable QtTwin::DestructionEffect::debris
    The pieces. advance() removes each when its lifetime is over.
*/

/*!
    Returns the effect for \a entity of \a kind with the extent \a box, with \a pieces pieces of
    debris thrown from random points of the box at speeds that scale with its size, from the random
    \a seed. The same arguments always give the same effect.
*/
DestructionEffect DestructionEffect::forEntity(const EntityState &entity, EntityKind kind, const BoundingBox &box,
                                               quint32 seed, int pieces) {
    DestructionEffect e;
    e.origin = PhysicsEngine::fromBodyFrame(entity, box.centre);
    const double size = box.radius();
    e.fireballRadius = std::max(0.5, size * 1.5);
    e.fireballDuration = kind == EntityKind::Drone || kind == EntityKind::Person ? 0.4 : 1.5;
    e.smokeDuration = kind == EntityKind::Drone || kind == EntityKind::Person ? 5.0 : 120.0;
    QRandomGenerator rng(seed);
    const Vector3d up = Ellipsoid::wgs84()
                            .surfaceNormal(entity.geoPosition().latitude, entity.geoPosition().longitude);
    const double speed = std::clamp(4.0 * size, 3.0, 25.0);
    for (int i = 0; i < pieces; ++i) {
        Debris d;
        const Vector3d local(box.centre.x + (rng.generateDouble() * 2.0 - 1.0) * box.halfExtents.x,
                             box.centre.y + (rng.generateDouble() * 2.0 - 1.0) * box.halfExtents.y,
                             box.centre.z + (rng.generateDouble() * 2.0 - 1.0) * box.halfExtents.z);
        d.position = PhysicsEngine::fromBodyFrame(entity, local);
        Vector3d outward = (d.position - e.origin).normalized();
        if (outward == Vector3d())
            outward = up;
        const double s = speed * (0.5 + rng.generateDouble());
        d.velocity = entity.velocity + (outward + up * (0.5 + rng.generateDouble())).normalized() * s;
        d.angularVelocity = Vector3d((rng.generateDouble() * 2.0 - 1.0) * 6.0, (rng.generateDouble() * 2.0 - 1.0) * 6.0,
                                     (rng.generateDouble() * 2.0 - 1.0) * 6.0);
        d.size = std::max(0.05, size * (0.05 + 0.15 * rng.generateDouble()));
        d.lifetime = 2.0 + 4.0 * rng.generateDouble();
        e.debris.append(d);
    }
    return e;
}

/*!
    Moves the debris on by \a dt seconds with \a physics, each piece as a slab with a drag
    coefficient of 1.2, and drops pieces whose lifetime is over or that reached the terrain.
*/
void DestructionEffect::advance(const PhysicsEngine &physics, double dt) {
    for (qsizetype i = debris.size() - 1; i >= 0; --i) {
        Debris &d = debris[i];
        d.lifetime -= dt;
        if (d.lifetime <= 0.0) {
            debris.removeAt(i);
            continue;
        }
        PointMass body;
        body.position = d.position;
        body.velocity = d.velocity;
        body.mass = std::max(0.01, 50.0 * d.size * d.size * d.size); // a slab of light alloy
        body.dragCoefficient = 1.2;
        body.referenceArea = d.size * d.size;
        physics.step(body, dt);
        if (physics.heightAboveTerrain(body.position) <= 0.0) {
            d.velocity = Vector3d();
            d.angularVelocity = Vector3d();
            continue; // lies on the ground until its lifetime ends
        }
        d.position = body.position;
        d.velocity = body.velocity;
    }
}

/*!
    \class QtTwin::Elimination
    \inmodule NtTwin
    \brief The Elimination struct reports that an entity was destroyed, and by whom.

    The owner of a destroyed entity sends it to every peer of the session, so each can show
    "ALPHA-1 (alice) was eliminated by BRAVO-2 (bob)" and keep score.

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DamageModel
*/

/*!
    \variable QtTwin::Elimination::victim
    The entity destroyed.
*/

/*!
    \variable QtTwin::Elimination::victimMarking
    Its marking.
*/

/*!
    \variable QtTwin::Elimination::victimOwner
    The peer that owned it.
*/

/*!
    \variable QtTwin::Elimination::victimOwnerName
    That peer's name.
*/

/*!
    \variable QtTwin::Elimination::killer
    The entity that fired the shot, or an invalid ID if the cause was not a shot.
*/

/*!
    \variable QtTwin::Elimination::killerMarking
    Its marking.
*/

/*!
    \variable QtTwin::Elimination::killerOwner
    The peer that owns the killer, 0 if none.
*/

/*!
    \variable QtTwin::Elimination::killerOwnerName
    That peer's name.
*/

/*!
    \variable QtTwin::Elimination::event
    The number of the shot's event, 0 if none.
*/

/*!
    \variable QtTwin::Elimination::time
    The session time of the elimination in milliseconds.
*/

/*!
    \fn bool QtTwin::Elimination::isValid() const
    Returns \c true if a victim is named.
*/

/*!
    \fn bool QtTwin::Elimination::isSelfInflicted() const
    Returns \c true if the killer belongs to the same peer as the victim.
*/

/*!
    Returns the report as text, such as "ALPHA-1 (alice) was eliminated by BRAVO-2 (bob)".
*/
QString Elimination::toString() const {
    const auto name = [](const QString &marking, const QString &owner, const DisEntityId &id) {
        QString s = marking.isEmpty() ? id.toString() : marking;
        if (!owner.isEmpty())
            s += QStringLiteral(" (%1)").arg(owner);
        return s;
    };
    const QString who = name(victimMarking, victimOwnerName, victim);
    if (!killer.isValid())
        return QStringLiteral("%1 was destroyed").arg(who);
    return QStringLiteral("%1 was eliminated by %2").arg(who, name(killerMarking, killerOwnerName, killer));
}

} // namespace QtTwin
