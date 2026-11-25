// MIT License - see LICENSE
#include <QtSensors/qsensorbackend.h>
#include <QtSensors/qsensormanager.h>

#include "qsensorbackend_p.h"

#include <QtCore/qlist.h>

#include <cmath>
#include <mutex>

namespace QtSensorsPrivate {

namespace {

struct Registration {
    QByteArray type;
    QByteArray identifier;
    QSensorBackendFactory *factory;
};

struct Registry {
    std::mutex mutex;
    QList<Registration> backends;
    QList<QPair<QByteArray, QByteArray>> defaults; // type, identifier
};

Registry &registry() {
    static Registry r;
    return r;
}

} // namespace

QList<QByteArray> registeredIdentifiers(const QByteArray &type) {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    QList<QByteArray> ids;
    for (const Registration &reg : r.backends)
        if (reg.type == type)
            ids.append(reg.identifier);
    for (const auto &def : r.defaults) {
        if (def.first == type && ids.removeAll(def.second) > 0) {
            ids.prepend(def.second);
            break;
        }
    }
    return ids;
}

QList<QByteArray> registeredTypes() {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    QList<QByteArray> types;
    for (const Registration &reg : r.backends)
        if (!types.contains(reg.type))
            types.append(reg.type);
    return types;
}

QSensorBackend *createRegisteredBackend(QSensor *sensor, const QByteArray &identifier) {
    QSensorBackendFactory *factory = nullptr;
    {
        Registry &r = registry();
        const std::lock_guard<std::mutex> lock(r.mutex);
        for (const Registration &reg : r.backends) {
            if (reg.type == sensor->type() && reg.identifier == identifier) {
                factory = reg.factory;
                break;
            }
        }
    }
    // Called without the lock: the factory may call back into the manager.
    return factory ? factory->createBackend(sensor) : nullptr;
}

} // namespace QtSensorsPrivate

/*!
    \class QSensorBackendFactory
    \inmodule NtSensors
    \brief The QSensorBackendFactory class makes the backends of sensors that an application provides.
    \since 6.0

    Reimplement createBackend() and register the factory with QSensorManager::registerBackend().

    \differences
    None.
    \enddifferences

    \sa QSensorManager, QSensorBackend
*/

/*!
    Constructs a factory.
*/
QSensorBackendFactory::QSensorBackendFactory() = default;

/*!
    Destroys the factory. Unregister it first.
*/
QSensorBackendFactory::~QSensorBackendFactory() = default;

/*!
    \fn QSensorBackend *QSensorBackendFactory::createBackend(QSensor *sensor)

    Returns a new backend for \a sensor, which takes ownership of it, or \c nullptr if there is none.
*/

/*!
    \class QSensorManager
    \inmodule NtSensors
    \brief The QSensorManager class registers the backends of sensors that an application provides.
    \since 6.0

    A backend reads a sensor that Nt does not know, such as a chip on a bus that the application
    drives itself. Register a factory for the type and an identifier of your choosing; a QSensor of
    the type then lists the identifier and, when it is the default, uses the backend:

    \code
    class ThermocoupleFactory : public QSensorBackendFactory {
    public:
        QSensorBackend *createBackend(QSensor *sensor) override { return new ThermocoupleBackend(sensor); }
    };

    static ThermocoupleFactory factory;
    QSensorManager::registerBackend(QAmbientTemperatureSensor::sensorType, "max31855", &factory);
    \endcode

    The default backend of a type is the one set with setDefaultBackend(), or else the first
    registered one. Registered backends come before the sensors that Nt reads itself.

    \differences
    There are no sensor plugins and no configuration file: backends are registered from code.
    \enddifferences

    \sa QSensorBackend, QSensorBackendFactory
*/

/*!
    Registers \a factory, which the manager does not take ownership of, to make the backend of the
    sensor \a identifier of \a type. A second registration of the same type and identifier is
    ignored.
*/
void QSensorManager::registerBackend(const QByteArray &type, const QByteArray &identifier,
                                     QSensorBackendFactory *factory) {
    if (!factory || isBackendRegistered(type, identifier))
        return;
    QtSensorsPrivate::Registry &r = QtSensorsPrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    r.backends.append({type, identifier, factory});
}

/*!
    Unregisters the backend \a identifier of \a type. Sensors that use it keep their backend.
*/
void QSensorManager::unregisterBackend(const QByteArray &type, const QByteArray &identifier) {
    QtSensorsPrivate::Registry &r = QtSensorsPrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    r.backends.removeIf(
        [&](const QtSensorsPrivate::Registration &reg) { return reg.type == type && reg.identifier == identifier; });
}

/*!
    Returns \c true if a backend \a identifier of \a type is registered.
*/
bool QSensorManager::isBackendRegistered(const QByteArray &type, const QByteArray &identifier) {
    QtSensorsPrivate::Registry &r = QtSensorsPrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    for (const QtSensorsPrivate::Registration &reg : r.backends)
        if (reg.type == type && reg.identifier == identifier)
            return true;
    return false;
}

/*!
    Returns a new backend for \a sensor: the registered one its identifier names, or the default of
    its type. Returns \c nullptr if none is registered. QSensor calls it when it connects.
*/
QSensorBackend *QSensorManager::createBackend(QSensor *sensor) {
    if (!sensor)
        return nullptr;
    QByteArray identifier = sensor->identifier();
    const QList<QByteArray> ids = QtSensorsPrivate::registeredIdentifiers(sensor->type());
    if (identifier.isEmpty() && !ids.isEmpty())
        identifier = ids.first();
    if (!ids.contains(identifier))
        return nullptr;
    return QtSensorsPrivate::createRegisteredBackend(sensor, identifier);
}

/*!
    Makes the registered backend \a identifier the default of \a type.
*/
void QSensorManager::setDefaultBackend(const QByteArray &type, const QByteArray &identifier) {
    QtSensorsPrivate::Registry &r = QtSensorsPrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    r.defaults.removeIf([&](const QPair<QByteArray, QByteArray> &def) { return def.first == type; });
    r.defaults.append({type, identifier});
}

/*!
    \class QSensorBackend
    \inmodule NtSensors
    \brief The QSensorBackend class reads a sensor for a QSensor, for sensors that an application provides.
    \since 6.0

    Subclass it, fill a reading of the type of the sensor, and call newReadingAvailable() each time
    it has a new value; register a QSensorBackendFactory that makes it with QSensorManager:

    \code
    class ThermocoupleBackend : public QSensorBackend {
    public:
        explicit ThermocoupleBackend(QSensor *sensor) : QSensorBackend(sensor) {
            m_reading = setReading<QAmbientTemperatureReading>(nullptr);
            setDescription(QStringLiteral("MAX31855 on SPI 1"));
            addDataRate(1, 10);
            QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
                m_reading->setTemperature(readThermocouple());
                m_reading->setTimestamp(microsecondsNow());
                newReadingAvailable();
            });
        }
        void start() override { m_timer.start(1000 / qMax(sensor()->dataRate(), 1)); }
        void stop() override { m_timer.stop(); }

    private:
        QAmbientTemperatureReading *m_reading;
        QTimer m_timer;
    };
    \endcode

    \differences
    newReadingAvailable() copies the reading into the reading of the sensor through its filters
    synchronously. A timestamp of 0 is replaced by the current time.
    \enddifferences

    \sa QSensorManager
*/

/*!
    Constructs a backend for \a sensor, with the given \a parent.
*/
QSensorBackend::QSensorBackend(QSensor *sensor, QObject *parent) : QObject(parent), m_sensor(sensor) {}

/*!
    Destroys the backend.
*/
QSensorBackend::~QSensorBackend() = default;

/*!
    \fn void QSensorBackend::start()

    Starts reading the sensor. Call sensorStopped() if it cannot start.
*/

/*!
    \fn void QSensorBackend::stop()

    Stops reading the sensor.
*/

/*!
    \fn template <typename T> T *QSensorBackend::setReading(T *reading)

    Sets the reading that the backend fills to \a reading, or to a new reading of class \c T that
    the backend owns if \a reading is \c nullptr, and returns it.
*/

/*!
    Returns \c true if the backend supports \a feature. The default returns \c false.
*/
bool QSensorBackend::isFeatureSupported(QSensor::Feature feature) const {
    (void)feature;
    return false;
}

/*!
    Adds a range of data rates from \a min to \a max readings a second.
*/
void QSensorBackend::addDataRate(qreal min, qreal max) {
    QtSensorsPrivate::SensorAccess::addDataRate(m_sensor, int(std::lround(min)), int(std::lround(max)));
}

/*!
    Adds the data rates of \a otherSensor.
*/
void QSensorBackend::setDataRates(const QSensor *otherSensor) {
    if (!otherSensor)
        return;
    for (const qrange &range : otherSensor->availableDataRates())
        QtSensorsPrivate::SensorAccess::addDataRate(m_sensor, range.first, range.second);
}

/*!
    Adds an output range from \a min to \a max with a resolution of \a accuracy.
*/
void QSensorBackend::addOutputRange(qreal min, qreal max, qreal accuracy) {
    QtSensorsPrivate::SensorAccess::addOutputRange(m_sensor, {min, max, accuracy});
}

/*!
    Sets the description of the sensor to \a description.
*/
void QSensorBackend::setDescription(const QString &description) {
    QtSensorsPrivate::SensorAccess::setDescription(m_sensor, description);
}

/*!
    Returns the sensor the backend reads for.
*/
QSensor *QSensorBackend::sensor() const { return m_sensor; }

/*!
    Returns the reading that the backend fills, as set with setReading().
*/
QSensorReading *QSensorBackend::reading() const { return m_reading; }

void QSensorBackend::setDeviceReading(QSensorReading *reading) { m_reading = reading; }

/*!
    Reports the values in reading() to the sensor, which runs its filters and emits
    QSensor::readingChanged() if they let the reading through.
*/
void QSensorBackend::newReadingAvailable() {
    if (m_reading)
        QtSensorsPrivate::SensorAccess::deliver(m_sensor, QtSensorsPrivate::readingValues(m_reading),
                                                m_reading->timestamp());
}

/*!
    Reports that the sensor stopped, or could not start.
*/
void QSensorBackend::sensorStopped() { QtSensorsPrivate::SensorAccess::stopped(m_sensor); }

/*!
    Reports that the sensor is in use elsewhere if \a busy is \c true, or free again otherwise.
*/
void QSensorBackend::sensorBusy(bool busy) { QtSensorsPrivate::SensorAccess::setBusy(m_sensor, busy); }

/*!
    Reports that the sensor failed with \a error.
*/
void QSensorBackend::sensorError(int error) { QtSensorsPrivate::SensorAccess::error(m_sensor, error); }

#include "moc_qsensorbackend.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
