// MIT License - see LICENSE
#pragma once

#include "qtwinentity.h"

namespace QtTwin {

struct DeadReckoningThresholds {
    double position = 1.0;
    double orientation = 3.0;
    qint64 heartbeat = 5000;
    qint64 minimumInterval = 50;
    double timeoutMultiplier = 2.4;

    qint64 staleTimeout() const { return static_cast<qint64>(static_cast<double>(heartbeat) * timeoutMultiplier); }
    static DeadReckoningThresholds forKind(EntityKind kind);
};

class DeadReckoning {
public:
    enum class UpdateReason { None, RateLimited, Threshold, Changed, Heartbeat };

    static EntityState extrapolate(const EntityState &state, qint64 time);
    static UpdateReason updateReason(const EntityState &lastSent, const EntityState &current,
                                     const DeadReckoningThresholds &thresholds);
    static double orientationDifference(const EulerAngles &a, const EulerAngles &b);
    static EntityState blend(const EntityState &from, const EntityState &to, double t);
};

} // namespace QtTwin
