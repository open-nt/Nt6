// MIT License - see LICENSE
//
// The sensor backend on platforms that Nt Sensors does not support yet: no sensors.

#include "qsensorbackend_p.h"

#include <cerrno>

namespace QtSensorsPrivate {

namespace {

class NullSensorBackend : public SensorBackend {
public:
    QList<SensorInfo> sensors() override { return {}; }
    std::unique_ptr<SensorDevice> open(const QByteArray &, const QByteArray &, int *error) override {
        *error = -ENODEV;
        return nullptr;
    }
};

} // namespace

std::unique_ptr<SensorBackend> createPlatformSensorBackend() { return std::make_unique<NullSensorBackend>(); }

} // namespace QtSensorsPrivate
