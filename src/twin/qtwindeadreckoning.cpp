// MIT License - see LICENSE
#include "qtwindeadreckoning.h"

#include "qtwinrotation_p.h"

#include <algorithm>
#include <cmath>

namespace QtTwin {

namespace {

constexpr double kDegrees = 3.14159265358979323846 / 180.0;

bool rotates(DeadReckoningAlgorithm a) {
    return a == DeadReckoningAlgorithm::Rpw || a == DeadReckoningAlgorithm::Rvw || a == DeadReckoningAlgorithm::Rpb ||
           a == DeadReckoningAlgorithm::Rvb;
}

bool accelerates(DeadReckoningAlgorithm a) {
    return a == DeadReckoningAlgorithm::Rvw || a == DeadReckoningAlgorithm::Fvw || a == DeadReckoningAlgorithm::Rvb ||
           a == DeadReckoningAlgorithm::Fvb;
}

bool moves(DeadReckoningAlgorithm a) {
    return a != DeadReckoningAlgorithm::Other && a != DeadReckoningAlgorithm::Static;
}

} // namespace

/*!
    \class QtTwin::DeadReckoningThresholds
    \inmodule NtTwin
    \brief The DeadReckoningThresholds struct says when the owner of an entity must send an update.

    The owner of an entity runs the same prediction as its receivers and sends a new state when the
    prediction is off by more than \l position metres or \l orientation degrees, but at most once
    every \l minimumInterval milliseconds. If nothing has been sent for \l heartbeat milliseconds,
    it sends the state anyway, so receivers know that a parked entity still exists. A receiver
    that hears nothing for staleTimeout() marks the entity stale.

    \differences
    Qt has no such type; this struct is part of Nt Twin. The defaults follow DIS: a 5 second
    heartbeat and a timeout of 2.4 heartbeats.
    \enddifferences

    \sa QtTwin::DeadReckoning
*/

/*!
    \variable QtTwin::DeadReckoningThresholds::position
    The largest error of the predicted position, in metres, before an update is sent.
*/

/*!
    \variable QtTwin::DeadReckoningThresholds::orientation
    The largest error of the predicted orientation, in degrees, before an update is sent.
*/

/*!
    \variable QtTwin::DeadReckoningThresholds::heartbeat
    The longest time without an update, in milliseconds.
*/

/*!
    \variable QtTwin::DeadReckoningThresholds::minimumInterval
    The shortest time between two updates, in milliseconds.
*/

/*!
    \variable QtTwin::DeadReckoningThresholds::timeoutMultiplier
    How many heartbeats a receiver waits before it marks an entity stale.
*/

/*!
    \fn qint64 QtTwin::DeadReckoningThresholds::staleTimeout() const
    Returns how long a receiver waits for an update before it marks the entity stale, in
    milliseconds: \l heartbeat times \l timeoutMultiplier.
*/

/*!
    Returns the default thresholds for \a kind:

    \table
    \header \li Kind \li Position \li Orientation \li Max rate \li Heartbeat
    \row \li Person \li 0.3 m \li 5° \li 10 Hz \li 5 s
    \row \li Ground vehicle \li 0.5 m \li 3° \li 20 Hz \li 5 s
    \row \li Aircraft \li 2 m \li 3° \li 30 Hz \li 5 s
    \row \li Drone \li 0.2 m \li 3° \li 30 Hz \li 5 s
    \row \li Boat \li 0.5 m \li 3° \li 10 Hz \li 5 s
    \endtable
*/
DeadReckoningThresholds DeadReckoningThresholds::forKind(EntityKind kind) {
    DeadReckoningThresholds t;
    switch (kind) {
    case EntityKind::Person:
        t.position = 0.3;
        t.orientation = 5.0;
        t.minimumInterval = 100;
        break;
    case EntityKind::GroundVehicle:
        t.position = 0.5;
        t.minimumInterval = 50;
        break;
    case EntityKind::Aircraft:
        t.position = 2.0;
        t.minimumInterval = 33;
        break;
    case EntityKind::Drone:
        t.position = 0.2;
        t.minimumInterval = 33;
        break;
    case EntityKind::Boat:
        t.position = 0.5;
        t.minimumInterval = 100;
        break;
    }
    return t;
}

/*!
    \class QtTwin::DeadReckoning
    \inmodule NtTwin
    \brief The DeadReckoning class predicts where an entity is between updates, and decides when
    its owner must send one.

    Only the owner of an entity sends its state. Every receiver extrapolates the last state with
    the algorithm the owner chose, so an entity that moves steadily costs almost nothing, and one
    that stands still costs one heartbeat every few seconds.

    \code
    // The owner, every frame:
    if (QtTwin::DeadReckoning::updateReason(lastSent, current, thresholds)
            >= QtTwin::DeadReckoning::UpdateReason::Threshold)
        send(current), lastSent = current;

    // A receiver, every frame:
    const QtTwin::EntityState now = QtTwin::DeadReckoning::extrapolate(lastReceived, sessionTime);
    \endcode

    \differences
    Qt has no dead reckoning; this class is part of Nt Twin. The body-axis algorithms of DIS are
    extrapolated as their world-axis equivalents.
    \enddifferences

    \sa QtTwin::DeadReckoningThresholds
*/

/*!
    \enum QtTwin::DeadReckoning::UpdateReason

    Why the owner of an entity should, or should not, send its state now.

    \value None The prediction is good enough.
    \value RateLimited The state should be sent, but the last update was too recent.
    \value Threshold The prediction is off by more than a threshold.
    \value Changed The appearance or an articulated part has changed.
    \value Heartbeat No update has been sent for a heartbeat interval.
*/

/*!
    Returns \a state predicted to the session time \a time (milliseconds) with its dead-reckoning
    algorithm. The result's time is \a time; its velocity includes the acceleration if the
    algorithm uses it.
*/
EntityState DeadReckoning::extrapolate(const EntityState &state, qint64 time) {
    EntityState r = state;
    r.time = time;
    const double dt = static_cast<double>(time - state.time) / 1000.0;
    const DeadReckoningAlgorithm a = state.deadReckoning;
    if (!moves(a) || dt == 0.0)
        return r;
    if (accelerates(a)) {
        r.position = state.position + state.velocity * dt + state.acceleration * (0.5 * dt * dt);
        r.velocity = state.velocity + state.acceleration * dt;
    } else {
        r.position = state.position + state.velocity * dt;
    }
    if (rotates(a)) {
        // The body turns about its own axes, so the new axes are the old ones rotated in body coordinates.
        const Rotation::Matrix turn = Rotation::axisAngle(state.angularVelocity * dt);
        r.orientation = Rotation::eulerFromAxes(
            Rotation::multiply(Rotation::transposed(turn), Rotation::bodyAxes(state.orientation)));
    }
    return r;
}

/*!
    Returns why the owner of an entity should send \a current, given the state \a lastSent that
    receivers are predicting from, and the \a thresholds of the entity. Send when the result is
    Threshold, Changed or Heartbeat.
*/
DeadReckoning::UpdateReason DeadReckoning::updateReason(const EntityState &lastSent, const EntityState &current,
                                                        const DeadReckoningThresholds &thresholds) {
    const qint64 elapsed = current.time - lastSent.time;
    UpdateReason reason = UpdateReason::None;
    if (elapsed >= thresholds.heartbeat) {
        reason = UpdateReason::Heartbeat;
    } else if (current.appearance != lastSent.appearance || current.articulatedParts != lastSent.articulatedParts ||
               current.deadReckoning != lastSent.deadReckoning) {
        reason = UpdateReason::Changed;
    } else {
        const EntityState predicted = extrapolate(lastSent, current.time);
        if ((predicted.position - current.position).length() > thresholds.position ||
            orientationDifference(predicted.orientation, current.orientation) > thresholds.orientation)
            reason = UpdateReason::Threshold;
    }
    if (reason != UpdateReason::None && reason != UpdateReason::Heartbeat && elapsed < thresholds.minimumInterval)
        return UpdateReason::RateLimited;
    return reason;
}

/*!
    Returns the angle, in degrees, of the rotation between the orientations \a a and \a b.
*/
double DeadReckoning::orientationDifference(const EulerAngles &a, const EulerAngles &b) {
    return Rotation::angleBetween(Rotation::bodyAxes(a), Rotation::bodyAxes(b)) / kDegrees;
}

/*!
    Returns a state between \a from (\a t = 0) and \a to (\a t = 1): positions and velocities are
    interpolated linearly and orientations along the shortest rotation. Everything else, including
    the time, comes from \a to. A receiver uses this to glide to a new update instead of jumping.
*/
EntityState DeadReckoning::blend(const EntityState &from, const EntityState &to, double t) {
    const double s = std::clamp(t, 0.0, 1.0);
    EntityState r = to;
    r.position = from.position + (to.position - from.position) * s;
    r.velocity = from.velocity + (to.velocity - from.velocity) * s;
    const Rotation::Quaternion q =
        Rotation::slerp(Rotation::quaternionFromMatrix(Rotation::bodyAxes(from.orientation)),
                        Rotation::quaternionFromMatrix(Rotation::bodyAxes(to.orientation)), s);
    r.orientation = Rotation::eulerFromAxes(Rotation::matrixFromQuaternion(q));
    return r;
}

} // namespace QtTwin
