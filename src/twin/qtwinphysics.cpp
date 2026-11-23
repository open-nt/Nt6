// MIT License - see LICENSE
#include "qtwinphysics.h"

#include "qtwindeadreckoning.h"
#include "qtwinrotation_p.h"

#include <algorithm>
#include <cmath>

namespace QtTwin {

namespace {

constexpr double kDegrees = 3.14159265358979323846 / 180.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kAimIterations = 24;

struct Derivative {
    Vector3d velocity;
    Vector3d acceleration;
};

} // namespace

/*!
    \class QtTwin::Environment
    \inmodule NtTwin
    \brief The Environment struct is the air and gravity a QtTwin::PhysicsEngine works in.

    \differences
    Qt has no physics; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::Environment::atmosphere
    The atmosphere, standard by default.
*/

/*!
    \variable QtTwin::Environment::wind
    The wind, calm by default.
*/

/*!
    \variable QtTwin::Environment::gravity
    Whether gravity acts; on by default. Off is useful to check a drag model on its own.
*/

/*!
    \variable QtTwin::Environment::drag
    Whether air resistance acts; on by default. Off gives vacuum ballistics.
*/

/*!
    \class QtTwin::PointMass
    \inmodule NtTwin
    \brief The PointMass struct is a body in flight: a projectile, a dropped store or a drone that
    has lost power.

    The body has a position and velocity in Earth-centred, Earth-fixed coordinates, a mass and a
    drag coefficient with its reference area. A QtTwin::PhysicsEngine moves it under gravity, air
    resistance and wind. A body with a drag coefficient or reference area of 0 feels no drag.

    \code
    QtTwin::PointMass round = QtTwin::PointMass::sphere(0.012, 0.0127, 0.3);   // 12 g, 12.7 mm, Cd 0.3
    round.position = muzzle;
    round.velocity = QtTwin::PhysicsEngine::launchVelocity(muzzle, 900.0, azimuth, elevation);
    \endcode

    \differences
    Qt has no physics; this struct is part of Nt Twin. The drag coefficient is constant; it does
    not vary with Mach number.
    \enddifferences

    \sa QtTwin::PhysicsEngine
*/

/*!
    \variable QtTwin::PointMass::position
    The position in Earth-centred, Earth-fixed coordinates, in metres.
*/

/*!
    \variable QtTwin::PointMass::velocity
    The velocity relative to the Earth, in metres per second.
*/

/*!
    \variable QtTwin::PointMass::time
    The time in seconds, advanced by QtTwin::PhysicsEngine::step().
*/

/*!
    \variable QtTwin::PointMass::mass
    The mass in kilograms, 1 by default.
*/

/*!
    \variable QtTwin::PointMass::dragCoefficient
    The drag coefficient, 0 by default: a sphere is about 0.47, a bullet 0.3, a streamlined shell
    0.15 and a person 1.0.
*/

/*!
    \variable QtTwin::PointMass::referenceArea
    The frontal area the drag coefficient refers to, in square metres.
*/

/*!
    \fn double QtTwin::PointMass::kineticEnergy() const
    Returns the kinetic energy in joules.
*/

/*!
    \fn double QtTwin::PointMass::speed() const
    Returns the speed in metres per second.
*/

/*!
    Returns a sphere of \a mass kilograms and \a diameter metres with the given \a dragCoefficient,
    at rest at the origin.
*/
PointMass PointMass::sphere(double mass, double diameter, double dragCoefficient) {
    PointMass p;
    p.mass = mass;
    p.dragCoefficient = dragCoefficient;
    p.referenceArea = kPi * diameter * diameter / 4.0;
    return p;
}

/*!
    \class QtTwin::TrajectorySample
    \inmodule NtTwin
    \brief The TrajectorySample struct is one point of a QtTwin::Trajectory.

    \differences
    Qt has no physics; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::TrajectorySample::time
    The time since launch in seconds.
*/

/*!
    \variable QtTwin::TrajectorySample::position
    The position in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::TrajectorySample::velocity
    The velocity in metres per second.
*/

/*!
    \class QtTwin::Trajectory
    \inmodule NtTwin
    \brief The Trajectory struct is the flight path QtTwin::PhysicsEngine::simulate() computed.

    \differences
    Qt has no physics; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \enum QtTwin::Trajectory::End
    How the flight ended.

    \value Flying It had not ended; the trajectory is empty.
    \value Ground The body reached the terrain.
    \value TimedOut The maximum time passed with the body still in the air.
    \value Hit The body reached its target.
*/

/*!
    \variable QtTwin::Trajectory::samples
    The path, sampled at the interval given to QtTwin::PhysicsEngine::simulate(), plus the final
    point.
*/

/*!
    \variable QtTwin::Trajectory::end
    How the flight ended.
*/

/*!
    \variable QtTwin::Trajectory::timeOfFlight
    The time in seconds from launch to the end.
*/

/*!
    \variable QtTwin::Trajectory::range
    The straight-line distance in metres from the launch point to the end point.
*/

/*!
    \variable QtTwin::Trajectory::apogee
    The greatest height above the ellipsoid reached, in metres.
*/

/*!
    \variable QtTwin::Trajectory::impact
    The end point in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \fn bool QtTwin::Trajectory::isEmpty() const
    Returns \c true if there are no samples.
*/

/*!
    Returns the position and velocity at \a time seconds after launch, interpolated between samples
    and clamped to the ends of the flight.
*/
TrajectorySample Trajectory::at(double time) const {
    if (samples.isEmpty())
        return TrajectorySample();
    if (time <= samples.first().time)
        return samples.first();
    if (time >= samples.last().time)
        return samples.last();
    const auto it = std::lower_bound(samples.cbegin(), samples.cend(), time,
                                     [](const TrajectorySample &s, double t) { return s.time < t; });
    const TrajectorySample &b = *it;
    const TrajectorySample &a = *(it - 1);
    const double span = b.time - a.time;
    const double f = span > 0.0 ? (time - a.time) / span : 0.0;
    TrajectorySample s;
    s.time = time;
    s.position = a.position + (b.position - a.position) * f;
    s.velocity = a.velocity + (b.velocity - a.velocity) * f;
    return s;
}

/*!
    \class QtTwin::BoundingBox
    \inmodule NtTwin
    \brief The BoundingBox struct is the extent of an entity in its body frame, for hit tests.

    The body frame has x forward, y to the right and z down, with the origin at the entity's
    position. forKind() gives a typical box for each kind of entity; a model catalogue can hold
    exact ones.

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::PhysicsEngine::segmentHitsEntity()
*/

/*!
    \variable QtTwin::BoundingBox::centre
    The centre of the box in the body frame, in metres. A ground vehicle whose position is on the
    ground has its centre above it, at negative z.
*/

/*!
    \variable QtTwin::BoundingBox::halfExtents
    Half the length, width and height of the box, in metres.
*/

/*!
    \fn constexpr QtTwin::BoundingBox::BoundingBox()
    Constructs a one-metre cube at the origin.
*/

/*!
    \fn constexpr QtTwin::BoundingBox::BoundingBox(const Vector3d &c, const Vector3d &half)
    Constructs the box centred at \a c with half extents \a half.
*/

/*!
    \fn double QtTwin::BoundingBox::radius() const
    Returns the radius of the sphere around the centre that contains the box.
*/

/*!
    Returns a typical box for an entity of \a kind: a person 0.6 by 0.6 by 1.8 m standing on the
    position, a ground vehicle 5 by 2.4 by 2 m on it, an aircraft 12 by 12 by 3 m around it, a drone
    1 by 1 by 0.4 m around it and a boat 7 by 2.6 by 2 m with the position at the waterline.
*/
BoundingBox BoundingBox::forKind(EntityKind kind) {
    switch (kind) {
    case EntityKind::Person:
        return BoundingBox(Vector3d(0.0, 0.0, -0.9), Vector3d(0.3, 0.3, 0.9));
    case EntityKind::GroundVehicle:
        return BoundingBox(Vector3d(0.0, 0.0, -1.0), Vector3d(2.5, 1.2, 1.0));
    case EntityKind::Aircraft:
        return BoundingBox(Vector3d(), Vector3d(6.0, 6.0, 1.5));
    case EntityKind::Drone:
        return BoundingBox(Vector3d(), Vector3d(0.5, 0.5, 0.2));
    case EntityKind::Boat:
        return BoundingBox(Vector3d(0.0, 0.0, -0.5), Vector3d(3.5, 1.3, 1.0));
    }
    return BoundingBox();
}

/*!
    \class QtTwin::FiringSolution
    \inmodule NtTwin
    \brief The FiringSolution struct is where to point a launcher so that its round meets a moving
    target, from QtTwin::PhysicsEngine::aim().

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::FiringSolution::valid
    Whether the round can reach the target within the tolerance. If not, the other fields hold the
    closest attempt.
*/

/*!
    \variable QtTwin::FiringSolution::azimuth
    The compass bearing to fire on, in degrees clockwise from north.
*/

/*!
    \variable QtTwin::FiringSolution::elevation
    The angle above the local horizon to fire at, in degrees.
*/

/*!
    \variable QtTwin::FiringSolution::timeOfFlight
    The time in seconds until the round meets the target.
*/

/*!
    \variable QtTwin::FiringSolution::miss
    The distance in metres by which the round would pass the target's position.
*/

/*!
    \variable QtTwin::FiringSolution::direction
    The unit vector of the launch velocity in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::FiringSolution::interceptPoint
    Where the round meets the target, in Earth-centred, Earth-fixed coordinates.
*/

/*!
    \variable QtTwin::FiringSolution::iterations
    How many trajectories were flown to find the solution.
*/

/*!
    \class QtTwin::PhysicsEngine
    \inmodule NtTwin
    \brief The PhysicsEngine class moves bodies in flight under gravity, air resistance and wind,
    and tests whether they hit anything.

    The engine integrates a QtTwin::PointMass in Earth-centred, Earth-fixed coordinates with the
    classical fourth-order Runge-Kutta method: gravity from QtTwin::Gravity, drag from the air
    density of the QtTwin::Atmosphere and the velocity relative to the QtTwin::WindModel. It serves
    both sides of a kinetic engagement: the launcher, which asks aim() where to point so that its
    round meets a moving drone, and every peer, which flies the round with step() and tests it
    against entities with segmentHitsEntity().

    \code
    QtTwin::Environment env;
    env.atmosphere = QtTwin::Atmosphere(100900.0, 283.15);
    env.wind = QtTwin::WindModel(8.0, 240.0);
    QtTwin::PhysicsEngine physics(env);
    physics.setTerrain([&](const QtTwin::GeoPosition &p) { return terrain.heightAt(p); });

    // The launcher asks where to point.
    const QtTwin::FiringSolution solution = physics.aim(muzzle, 900.0, round, drone.state);
    if (solution.valid)
        turret.pointAt(solution.azimuth, solution.elevation);

    // Everyone flies the round.
    QtTwin::PointMass shot = round;
    shot.position = muzzle;
    shot.velocity = solution.direction * 900.0;
    while (physics.heightAboveTerrain(shot.position) > 0.0) {
        const QtTwin::Vector3d before = shot.position;
        physics.step(shot, 1.0 / 120.0);
        if (QtTwin::PhysicsEngine::segmentHitsEntity(before, shot.position, drone.state, drone.box))
            break;
    }
    \endcode

    Entities that fly, such as a drone that has lost power, go through advance(), which takes and
    returns a QtTwin::EntityState.

    \differences
    Qt has no physics engine; this class is part of Nt Twin. Bodies are point masses: there is no
    lift, spin or rigid-body rotation, and no collision between bodies other than the segment tests.
    \enddifferences

    \sa QtTwin::Environment, QtTwin::Trajectory, QtTwin::DamageModel
*/

/*!
    \typedef QtTwin::PhysicsEngine::TerrainFunction
    A function returning the ellipsoidal height of the terrain in metres at a position.
*/

/*!
    \fn QtTwin::PhysicsEngine::PhysicsEngine()
    Constructs an engine with the standard atmosphere, calm air and the ellipsoid as terrain.
*/

/*!
    Constructs an engine working in \a environment.
*/
PhysicsEngine::PhysicsEngine(const Environment &environment) : m_environment(environment) {}

/*!
    \fn QtTwin::Environment QtTwin::PhysicsEngine::environment() const
    Returns the environment.
*/

/*!
    \fn void QtTwin::PhysicsEngine::setEnvironment(const Environment &environment)
    Sets the environment to \a environment.
*/

/*!
    \fn double QtTwin::PhysicsEngine::maximumStep() const
    Returns the longest integration step in seconds, 0.01 by default.
*/

/*!
    Sets the longest integration step to \a seconds. step() and advance() divide longer intervals
    into steps of at most this length. The default of 0.01 s keeps a 900 m/s round within
    millimetres of the exact path.
*/
void PhysicsEngine::setMaximumStep(double seconds) { m_maximumStep = std::max(1e-4, seconds); }

/*!
    \fn QtTwin::PhysicsEngine::TerrainFunction QtTwin::PhysicsEngine::terrain() const
    Returns the terrain function, or an empty function if the ellipsoid is the terrain.
*/

/*!
    \fn void QtTwin::PhysicsEngine::setTerrain(const TerrainFunction &heightAt)
    Sets the terrain to \a heightAt, which returns the ellipsoidal height of the ground at a
    position. simulate() and aim() end a flight there. Without one, the ground is the ellipsoid.
*/

/*!
    Returns the velocity of \a body relative to the air around it, in metres per second.
*/
Vector3d PhysicsEngine::airVelocity(const PointMass &body) const {
    return body.velocity - m_environment.wind.atEcef(body.position);
}

/*!
    Returns the Mach number of \a body: its speed through the air over the local speed of sound.
*/
double PhysicsEngine::mach(const PointMass &body) const {
    const GeoPosition geo = Ellipsoid::wgs84().fromEcef(body.position);
    return airVelocity(body).length() / m_environment.atmosphere.speedOfSound(geo.height);
}

/*!
    Returns the acceleration of \a body in its current state: gravity plus the drag of the air
    moving relative to it, in metres per second squared.
*/
Vector3d PhysicsEngine::acceleration(const PointMass &body) const {
    const GeoPosition geo = Ellipsoid::wgs84().fromEcef(body.position);
    Vector3d a;
    if (m_environment.gravity)
        a = Gravity::at(geo);
    if (m_environment.drag && body.dragCoefficient > 0.0 && body.referenceArea > 0.0 && body.mass > 0.0) {
        Vector3d relative = body.velocity;
        if (!m_environment.wind.isCalm()) {
            const Vector3d enu = m_environment.wind.at(geo.height);
            const Rotation::Matrix ned = Rotation::nedAxes(geo.latitude, geo.longitude);
            relative = relative - (Rotation::row(ned, 1) * enu.x + Rotation::row(ned, 0) * enu.y +
                                   Rotation::row(ned, 2) * -enu.z);
        }
        const double speed = relative.length();
        const double rho = m_environment.atmosphere.density(geo.height);
        const double k = 0.5 * rho * body.dragCoefficient * body.referenceArea / body.mass;
        a = a - relative * (k * speed);
    }
    return a;
}

/*!
    Moves \a body forward by \a dt seconds, in as many steps of at most maximumStep() as needed.
*/
void PhysicsEngine::step(PointMass &body, double dt) const {
    if (dt <= 0.0)
        return;
    const int steps = std::max(1, static_cast<int>(std::ceil(dt / m_maximumStep)));
    const double h = dt / steps;
    for (int i = 0; i < steps; ++i) {
        const auto derivative = [this, &body](const Vector3d &p, const Vector3d &v) {
            PointMass probe = body;
            probe.position = p;
            probe.velocity = v;
            return Derivative{v, acceleration(probe)};
        };
        const Derivative k1 = derivative(body.position, body.velocity);
        const Derivative k2 =
            derivative(body.position + k1.velocity * (h / 2.0), body.velocity + k1.acceleration * (h / 2.0));
        const Derivative k3 =
            derivative(body.position + k2.velocity * (h / 2.0), body.velocity + k2.acceleration * (h / 2.0));
        const Derivative k4 = derivative(body.position + k3.velocity * h, body.velocity + k3.acceleration * h);
        body.position = body.position + (k1.velocity + k2.velocity * 2.0 + k3.velocity * 2.0 + k4.velocity) * (h / 6.0);
        body.velocity = body.velocity +
                        (k1.acceleration + k2.acceleration * 2.0 + k3.acceleration * 2.0 + k4.acceleration) * (h / 6.0);
        body.time += h;
    }
}

/*!
    Moves \a body forward by \a duration seconds. This is step(); the name reads better for long
    intervals.
*/
void PhysicsEngine::advance(PointMass &body, double duration) const { step(body, duration); }

/*!
    \overload
    Returns \a state moved forward by \a duration seconds as a body with the mass and drag of
    \a properties: a drone that has lost power, or a store that was dropped. The orientation and
    appearance are kept; the position, velocity, acceleration and time are updated.
*/
EntityState PhysicsEngine::advance(const EntityState &state, const PointMass &properties, double duration) const {
    PointMass body = properties;
    body.position = state.position;
    body.velocity = state.velocity;
    body.time = 0.0;
    step(body, duration);
    EntityState result = state;
    result.position = body.position;
    result.velocity = body.velocity;
    result.acceleration = acceleration(body);
    result.time = state.time + static_cast<qint64>(std::llround(duration * 1000.0));
    return result;
}

/*!
    Returns the height in metres of the position \a ecef above the terrain, or above the ellipsoid
    without a terrain function.
*/
double PhysicsEngine::heightAboveTerrain(const Vector3d &ecef) const {
    const GeoPosition geo = Ellipsoid::wgs84().fromEcef(ecef);
    return geo.height - (m_terrain ? m_terrain(geo) : 0.0);
}

/*!
    Flies \a launch until it reaches the terrain or \a maximumTime seconds have passed, and returns
    the path sampled every \a sampleInterval seconds. The end point is interpolated to the ground.
*/
Trajectory PhysicsEngine::simulate(const PointMass &launch, double maximumTime, double sampleInterval) const {
    Trajectory t;
    PointMass body = launch;
    body.time = 0.0;
    const double interval = std::max(sampleInterval, m_maximumStep);
    double nextSample = 0.0;
    double previousHeight = heightAboveTerrain(body.position);
    PointMass previous = body;
    t.apogee = Ellipsoid::wgs84().fromEcef(body.position).height;
    t.end = Trajectory::End::TimedOut;
    while (body.time < maximumTime) {
        if (body.time >= nextSample - 1e-9) {
            t.samples.append(TrajectorySample{body.time, body.position, body.velocity});
            nextSample += interval;
        }
        previous = body;
        step(body, std::min(m_maximumStep, maximumTime - body.time));
        const double height = heightAboveTerrain(body.position);
        t.apogee = std::max(t.apogee, Ellipsoid::wgs84().fromEcef(body.position).height);
        if (height <= 0.0 && previousHeight > 0.0) {
            const double f = previousHeight / (previousHeight - height);
            body.position = previous.position + (body.position - previous.position) * f;
            body.velocity = previous.velocity + (body.velocity - previous.velocity) * f;
            body.time = previous.time + (body.time - previous.time) * f;
            t.end = Trajectory::End::Ground;
            break;
        }
        if (height <= 0.0) {
            t.end = Trajectory::End::Ground;
            break;
        }
        previousHeight = height;
    }
    t.samples.append(TrajectorySample{body.time, body.position, body.velocity});
    t.timeOfFlight = body.time;
    t.impact = body.position;
    t.range = (body.position - launch.position).length();
    return t;
}

/*!
    Returns the velocity of a round leaving \a launcher at \a speed metres per second on the compass
    bearing \a azimuth and \a elevation degrees above the local horizon, in Earth-centred,
    Earth-fixed coordinates.
*/
Vector3d PhysicsEngine::launchVelocity(const Vector3d &launcher, double speed, double azimuth, double elevation) {
    const GeoPosition geo = Ellipsoid::wgs84().fromEcef(launcher);
    const Rotation::Matrix ned = Rotation::nedAxes(geo.latitude, geo.longitude);
    const double az = azimuth * kDegrees, el = elevation * kDegrees;
    const Vector3d enu(std::sin(az) * std::cos(el), std::cos(az) * std::cos(el), std::sin(el));
    return (Rotation::row(ned, 1) * enu.x + Rotation::row(ned, 0) * enu.y + Rotation::row(ned, 2) * -enu.z) * speed;
}

/*!
    Returns where to point a launcher at \a launcher so that \a round, leaving at \a muzzleSpeed
    metres per second, meets \a target, which keeps moving as its dead reckoning predicts. The
    solution is refined by flying the round until it passes within \a tolerance metres of the target
    or the flight takes longer than \a maximumTime seconds. The low trajectory is found; a target
    out of range gives a solution that is not valid, with the closest miss.
*/
FiringSolution PhysicsEngine::aim(const Vector3d &launcher, double muzzleSpeed, const PointMass &round,
                                  const EntityState &target, double maximumTime, double tolerance) const {
    FiringSolution best;
    if (muzzleSpeed <= 0.0)
        return best;
    const LocalFrame frame(Ellipsoid::wgs84().fromEcef(launcher));
    const auto targetAt = [&](double seconds) {
        return DeadReckoning::extrapolate(target, target.time + static_cast<qint64>(std::llround(seconds * 1000.0)))
            .position;
    };
    double azimuth = 0.0, elevation = 0.0;
    {
        // The vacuum solution to the predicted position as the starting point.
        const double straight = (target.position - launcher).length() / muzzleSpeed;
        const Vector3d aimPoint = frame.toLocal(targetAt(straight));
        const double range = std::hypot(aimPoint.x, aimPoint.y);
        azimuth = std::atan2(aimPoint.x, aimPoint.y) / kDegrees;
        const double g = Gravity::standard, v2 = muzzleSpeed * muzzleSpeed;
        const double disc = v2 * v2 - g * (g * range * range + 2.0 * aimPoint.z * v2);
        if (disc >= 0.0 && range > 1e-9)
            elevation = std::atan((v2 - std::sqrt(disc)) / (g * range)) / kDegrees;
        else
            elevation = range > 1e-9 ? 45.0 : (aimPoint.z >= 0.0 ? 90.0 : -90.0);
    }
    for (int iteration = 1; iteration <= kAimIterations; ++iteration) {
        PointMass body = round;
        body.position = launcher;
        body.velocity = launchVelocity(launcher, muzzleSpeed, azimuth, elevation);
        body.time = 0.0;
        double closest = (targetAt(0.0) - launcher).length();
        double closestTime = 0.0;
        Vector3d closestBody = launcher;
        Vector3d closestTarget = targetAt(0.0);
        while (body.time < maximumTime) {
            const PointMass before = body;
            step(body, m_maximumStep);
            const Vector3d t1 = targetAt(body.time);
            const double d1 = (t1 - body.position).length();
            if (d1 < closest) {
                // Refine the closest approach within the step by sampling it.
                const Vector3d t0 = targetAt(before.time);
                const Vector3d relative0 = t0 - before.position;
                const Vector3d relative1 = t1 - body.position;
                const Vector3d dr = relative1 - relative0;
                const double len2 = dr.dotProduct(dr);
                const double f = len2 > 0.0 ? std::clamp(-relative0.dotProduct(dr) / len2, 0.0, 1.0) : 1.0;
                closestBody = before.position + (body.position - before.position) * f;
                closestTarget = t0 + (t1 - t0) * f;
                closestTime = before.time + (body.time - before.time) * f;
                closest = (closestTarget - closestBody).length();
            } else if (d1 > closest + 2.0 * tolerance && body.time > closestTime) {
                break; // past the target
            }
            if (heightAboveTerrain(body.position) <= 0.0)
                break;
        }
        if (iteration == 1 || closest < best.miss) {
            best.miss = closest;
            best.azimuth = std::fmod(azimuth + 360.0, 360.0);
            best.elevation = elevation;
            best.timeOfFlight = closestTime;
            best.direction = body.velocity.length() > 0.0 ? launchVelocity(launcher, 1.0, azimuth, elevation)
                                                          : Vector3d();
            best.interceptPoint = closestTarget;
            best.iterations = iteration;
        }
        if (closest <= tolerance) {
            best.valid = true;
            break;
        }
        // Correct the aim by the angles the miss subtends at the launcher.
        const Vector3d missLocal = frame.toLocal(closestTarget) - frame.toLocal(closestBody);
        const double distance = std::max(1.0, frame.toLocal(closestTarget).length());
        const double az = azimuth * kDegrees, el = elevation * kDegrees;
        const Vector3d forward(std::sin(az) * std::cos(el), std::cos(az) * std::cos(el), std::sin(el));
        const Vector3d right(std::cos(az), -std::sin(az), 0.0);
        const Vector3d up = right.crossProduct(forward);
        azimuth += std::atan2(missLocal.dotProduct(right), distance) / kDegrees;
        elevation += std::atan2(missLocal.dotProduct(up), distance) / kDegrees;
        elevation = std::clamp(elevation, -89.0, 89.0);
    }
    return best;
}

/*!
    Returns \c true if the segment from \a from to \a to passes through the sphere of \a radius at
    \a centre, and sets *\a fraction to where along the segment it enters, 0 at \a from and 1 at
    \a to, if \a fraction is not null.
*/
bool PhysicsEngine::segmentHitsSphere(const Vector3d &from, const Vector3d &to, const Vector3d &centre, double radius,
                                      double *fraction) {
    const Vector3d d = to - from;
    const Vector3d m = from - centre;
    const double a = d.dotProduct(d);
    const double b = 2.0 * m.dotProduct(d);
    const double c = m.dotProduct(m) - radius * radius;
    if (c <= 0.0) {
        if (fraction)
            *fraction = 0.0;
        return true; // starts inside
    }
    if (a <= 0.0)
        return false;
    const double disc = b * b - 4.0 * a * c;
    if (disc < 0.0)
        return false;
    const double t = (-b - std::sqrt(disc)) / (2.0 * a);
    if (t < 0.0 || t > 1.0)
        return false;
    if (fraction)
        *fraction = t;
    return true;
}

/*!
    Returns the position \a ecef in the body frame of \a entity: metres forward, right and down
    of its position, along its orientation.
*/
Vector3d PhysicsEngine::toBodyFrame(const EntityState &entity, const Vector3d &ecef) {
    const Rotation::Matrix m = Rotation::bodyAxes(entity.orientation);
    const Vector3d d = ecef - entity.position;
    return Vector3d(Rotation::row(m, 0).dotProduct(d), Rotation::row(m, 1).dotProduct(d),
                    Rotation::row(m, 2).dotProduct(d));
}

/*!
    Returns the point \a body, given in the body frame of \a entity, in Earth-centred, Earth-fixed
    coordinates.
*/
Vector3d PhysicsEngine::fromBodyFrame(const EntityState &entity, const Vector3d &body) {
    const Rotation::Matrix m = Rotation::bodyAxes(entity.orientation);
    return entity.position + Rotation::row(m, 0) * body.x + Rotation::row(m, 1) * body.y + Rotation::row(m, 2) * body.z;
}

/*!
    Returns \c true if the segment from \a from to \a to passes through \a box placed on \a entity,
    and sets *\a fraction to where along the segment it enters and *\a hitPoint to that point, if
    they are not null. Call it with the positions of a round before and after each step.
*/
bool PhysicsEngine::segmentHitsEntity(const Vector3d &from, const Vector3d &to, const EntityState &entity,
                                      const BoundingBox &box, double *fraction, Vector3d *hitPoint) {
    const Vector3d a = toBodyFrame(entity, from) - box.centre;
    const Vector3d b = toBodyFrame(entity, to) - box.centre;
    const Vector3d d = b - a;
    const double ac[3] = {a.x, a.y, a.z};
    const double dc[3] = {d.x, d.y, d.z};
    const double hc[3] = {box.halfExtents.x, box.halfExtents.y, box.halfExtents.z};
    double tEnter = 0.0, tExit = 1.0;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dc[i]) < 1e-12) {
            if (std::fabs(ac[i]) > hc[i])
                return false;
            continue;
        }
        double t1 = (-hc[i] - ac[i]) / dc[i];
        double t2 = (hc[i] - ac[i]) / dc[i];
        if (t1 > t2)
            std::swap(t1, t2);
        tEnter = std::max(tEnter, t1);
        tExit = std::min(tExit, t2);
        if (tEnter > tExit)
            return false;
    }
    if (fraction)
        *fraction = tEnter;
    if (hitPoint)
        *hitPoint = from + (to - from) * tEnter;
    return true;
}

} // namespace QtTwin
