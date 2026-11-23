// MIT License - see LICENSE
#pragma once

#include "qtwinatmosphere.h"
#include "qtwinentity.h"

#include <QtCore/qlist.h>

#include <functional>

namespace QtTwin {

struct Environment {
    Atmosphere atmosphere;
    WindModel wind;
    bool gravity = true;
    bool drag = true;
};

struct PointMass {
    Vector3d position;
    Vector3d velocity;
    double time = 0.0;
    double mass = 1.0;
    double dragCoefficient = 0.0;
    double referenceArea = 0.0;

    double kineticEnergy() const { return 0.5 * mass * velocity.dotProduct(velocity); }
    double speed() const { return velocity.length(); }
    static PointMass sphere(double mass, double diameter, double dragCoefficient = 0.47);
};

struct TrajectorySample {
    double time = 0.0;
    Vector3d position;
    Vector3d velocity;
};

struct Trajectory {
    enum class End : quint8 { Flying, Ground, TimedOut, Hit };

    QList<TrajectorySample> samples;
    End end = End::Flying;
    double timeOfFlight = 0.0;
    double range = 0.0;
    double apogee = 0.0;
    Vector3d impact;

    bool isEmpty() const { return samples.isEmpty(); }
    TrajectorySample at(double time) const;
};

struct BoundingBox {
    Vector3d centre;
    Vector3d halfExtents = Vector3d(0.5, 0.5, 0.5);

    constexpr BoundingBox() = default;
    constexpr BoundingBox(const Vector3d &c, const Vector3d &half) : centre(c), halfExtents(half) {}
    double radius() const { return halfExtents.length(); }
    static BoundingBox forKind(EntityKind kind);
};

struct FiringSolution {
    bool valid = false;
    double azimuth = 0.0;
    double elevation = 0.0;
    double timeOfFlight = 0.0;
    double miss = 0.0;
    Vector3d direction;
    Vector3d interceptPoint;
    int iterations = 0;
};

class PhysicsEngine {
public:
    using TerrainFunction = std::function<double(const GeoPosition &)>;

    PhysicsEngine() = default;
    explicit PhysicsEngine(const Environment &environment);

    Environment environment() const { return m_environment; }
    void setEnvironment(const Environment &environment) { m_environment = environment; }
    double maximumStep() const { return m_maximumStep; }
    void setMaximumStep(double seconds);
    TerrainFunction terrain() const { return m_terrain; }
    void setTerrain(const TerrainFunction &heightAt) { m_terrain = heightAt; }

    Vector3d acceleration(const PointMass &body) const;
    Vector3d airVelocity(const PointMass &body) const;
    double mach(const PointMass &body) const;
    void step(PointMass &body, double dt) const;
    void advance(PointMass &body, double duration) const;
    EntityState advance(const EntityState &state, const PointMass &properties, double duration) const;

    Trajectory simulate(const PointMass &launch, double maximumTime, double sampleInterval = 0.05) const;
    double heightAboveTerrain(const Vector3d &ecef) const;

    FiringSolution aim(const Vector3d &launcher, double muzzleSpeed, const PointMass &round, const EntityState &target,
                       double maximumTime = 60.0, double tolerance = 0.25) const;
    static Vector3d launchVelocity(const Vector3d &launcher, double speed, double azimuth, double elevation);

    static bool segmentHitsSphere(const Vector3d &from, const Vector3d &to, const Vector3d &centre, double radius,
                                  double *fraction = nullptr);
    static bool segmentHitsEntity(const Vector3d &from, const Vector3d &to, const EntityState &entity,
                                  const BoundingBox &box, double *fraction = nullptr, Vector3d *hitPoint = nullptr);
    static Vector3d toBodyFrame(const EntityState &entity, const Vector3d &ecef);
    static Vector3d fromBodyFrame(const EntityState &entity, const Vector3d &body);

private:
    Environment m_environment;
    double m_maximumStep = 0.01;
    TerrainFunction m_terrain;
};

} // namespace QtTwin
