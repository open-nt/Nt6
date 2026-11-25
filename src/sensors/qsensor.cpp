// MIT License - see LICENSE
#include <QtSensors/qsensor.h>

#include "qsensorbackend_p.h"

#include <QtSensors/qsensorbackend.h>

#include <QtCore/qmetaobject.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <mutex>

/*!
    \module NtSensors
    \title Nt Sensors C++ Classes
    \brief Classes for reading accelerometers, gyroscopes, magnetometers and environmental sensors.
    \cmaketarget Nt::Sensors

    Nt Sensors is a subset of Qt Sensors. QSensor and its subclasses, such as QAccelerometer and
    QPressureSensor, read a sensor of the machine at a data rate and report each value as a
    QSensorReading. On Linux the sensors are the Industrial I/O devices of the kernel, and on Zephyr
    the sensor drivers that the devicetree names.

    See \l{Using Nt Sensors} for a walk through.
*/

/*!
    \class qoutputrange
    \inmodule NtSensors
    \brief The qoutputrange struct describes a range that a sensor can measure in.
    \since 6.0

    \differences
    None.
    \enddifferences

    \sa QSensor::outputRanges()
*/

/*!
    \variable qoutputrange::minimum

    The lowest value of the range.
*/

/*!
    \variable qoutputrange::maximum

    The highest value of the range.
*/

/*!
    \variable qoutputrange::accuracy

    The resolution of the values in the range.
*/

namespace QtSensorsPrivate {

namespace {

std::mutex &backendMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unique_ptr<SensorBackend> &backendInstance() {
    static std::unique_ptr<SensorBackend> backend;
    return backend;
}

quint64 microsecondsNow() {
    return quint64(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

SensorBackend *sensorBackend() {
    const std::lock_guard<std::mutex> lock(backendMutex());
    auto &backend = backendInstance();
    if (!backend)
        backend = createDerivedSensorBackend(createPlatformSensorBackend());
    return backend.get();
}

void setSensorBackend(std::unique_ptr<SensorBackend> backend) {
    const std::lock_guard<std::mutex> lock(backendMutex());
    backendInstance() = backend ? createDerivedSensorBackend(std::move(backend)) : nullptr;
}

} // namespace QtSensorsPrivate

/*!
    \class QSensorReading
    \inmodule NtSensors
    \brief The QSensorReading class holds a value that a sensor reported.
    \since 6.0

    Each kind of sensor has a reading of its own, such as QAccelerometerReading, with a property for
    each part of the value. The reading of a sensor is updated in place each time
    QSensor::readingChanged() is emitted:

    \code
    QAccelerometer accelerometer;
    QObject::connect(&accelerometer, &QSensor::readingChanged, [&accelerometer] {
        const QAccelerometerReading *r = accelerometer.reading();
        qDebug() << r->timestamp() << r->x() << r->y() << r->z();
    });
    accelerometer.start();
    \endcode

    \differences
    The values are kept in the reading itself; there is no private data class and no
    \c copyValuesFrom().
    \enddifferences

    \sa QSensor
*/

/*!
    Constructs a reading with the given \a parent.
*/
QSensorReading::QSensorReading(QObject *parent) : QObject(parent) {}

/*!
    Destroys the reading.
*/
QSensorReading::~QSensorReading() = default;

/*!
    Returns when the value was measured, in microseconds from a fixed but arbitrary point. Only
    differences between timestamps are meaningful.
*/
quint64 QSensorReading::timestamp() const { return m_timestamp; }

/*!
    Sets the timestamp of the reading to \a timestamp, in microseconds.
*/
void QSensorReading::setTimestamp(quint64 timestamp) { m_timestamp = timestamp; }

/*!
    Returns the number of values the reading holds, which is the number of properties its type
    adds to QSensorReading.

    \sa value()
*/
int QSensorReading::valueCount() const {
    return metaObject()->propertyCount() - QSensorReading::staticMetaObject.propertyCount();
}

/*!
    Returns the value at \a index, counting the properties that the type of the reading adds, or
    an invalid QVariant if there is none.

    \sa valueCount()
*/
QVariant QSensorReading::value(int index) const {
    if (index < 0 || index >= valueCount())
        return QVariant();
    return metaObject()->property(QSensorReading::staticMetaObject.propertyCount() + index).read(this);
}

/*!
    \class QSensorFilter
    \inmodule NtSensors
    \brief The QSensorFilter class lets code look at, change or drop a reading before the sensor reports it.
    \since 6.0

    Reimplement filter() in a subclass, or in a subclass of a typed filter such as
    QAccelerometerFilter, and add it with QSensor::addFilter(). Filters run in the order they were
    added; when one returns \c false the reading is dropped and readingChanged() is not emitted.

    \code
    class LowPass : public QAccelerometerFilter {
    public:
        using QAccelerometerFilter::filter; // keeps -Woverloaded-virtual quiet
        bool filter(QAccelerometerReading *reading) override {
            m_x = 0.9 * m_x + 0.1 * reading->x();
            reading->setX(m_x);
            return true;
        }
    private:
        qreal m_x = 0;
    };
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSensor::addFilter()
*/

/*!
    Constructs a filter that is not added to a sensor.
*/
QSensorFilter::QSensorFilter() = default;

/*!
    Destroys the filter, removing it from its sensor.
*/
QSensorFilter::~QSensorFilter() {
    if (m_sensor)
        m_sensor->removeFilter(this);
}

/*!
    \fn bool QSensorFilter::filter(QSensorReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \variable QSensorFilter::m_sensor

    The sensor the filter is added to, or \c nullptr.
*/

/*!
    Records \a sensor as the sensor the filter is added to.
*/
void QSensorFilter::setSensor(QSensor *sensor) { m_sensor = sensor; }

class QSensorPrivate {
public:
    QSensorPrivate(QSensor *owner, const QByteArray &sensorType, QSensorReading *sensorReading)
        : q(owner), type(sensorType), reading(sensorReading) {
        reading->setParent(owner);
        scratch = QtSensorsPrivate::createReading(sensorType, owner);
        timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer, &QTimer::timeout, owner, [this] { sample(); });
    }

    void setError(int code) {
        error = code;
        emit q->sensorError(code);
    }

    void setActive(bool on) {
        if (active == on)
            return;
        active = on;
        emit q->activeChanged();
    }

    // Takes a sample of a built-in sensor.
    void sample() {
        if (!device)
            return;
        QList<qreal> values;
        int err = 0;
        if (!device->read(values, &err)) {
            if (err == -EAGAIN)
                return; // nothing new yet
            setError(err < 0 ? err : -EIO);
            q->stop();
            return;
        }
        deliver(values, QtSensorsPrivate::microsecondsNow());
    }

    // Reports values, from a built-in sensor or a QSensorBackend, unless a filter drops them.
    void deliver(QList<qreal> values, quint64 timestamp) {
        raw = values;
        if (adjuster)
            adjuster(values);
        if (skipDuplicates && haveLast && values == last)
            return;
        last = values;
        haveLast = true;
        // Filters see a copy, so that the reading of the sensor changes only for a value that is reported.
        QtSensorsPrivate::setReadingValues(scratch, values);
        scratch->setTimestamp(timestamp);
        for (QSensorFilter *filter : filters) {
            if (!filter->filter(scratch))
                return;
        }
        QtSensorsPrivate::setReadingValues(reading, QtSensorsPrivate::readingValues(scratch));
        reading->setTimestamp(timestamp);
        emit q->readingChanged();
    }

    int interval() const { return 1000 / std::clamp(dataRate > 0 ? dataRate : 10, 1, 1000); }

    QSensor *q;
    QByteArray type;
    QByteArray identifier;
    QSensorReading *reading;
    QSensorReading *scratch = nullptr;
    QList<QSensorFilter *> filters;
    std::unique_ptr<QtSensorsPrivate::SensorDevice> device; // a built-in sensor, polled by timer
    QSensorBackend *backend = nullptr;                      // or one registered with QSensorManager
    QtSensorsPrivate::SensorInfo info;
    std::function<void(QList<qreal> &)> adjuster;
    bool connected = false;
    bool active = false;
    bool busy = false;
    bool alwaysOn = false;
    bool skipDuplicates = false;
    bool haveLast = false;
    QList<qreal> last;
    QList<qreal> raw;
    int dataRate = 0;
    int outputRange = -1;
    int error = 0;
    QTimer timer;
};

namespace QtSensorsPrivate {

void SensorAccess::deliver(QSensor *sensor, const QList<qreal> &values, quint64 timestamp) {
    if (sensor->d->active)
        sensor->d->deliver(values, timestamp ? timestamp : microsecondsNow());
}

void SensorAccess::stopped(QSensor *sensor) { sensor->d->setActive(false); }

void SensorAccess::setBusy(QSensor *sensor, bool busy) {
    if (sensor->d->busy == busy)
        return;
    sensor->d->busy = busy;
    emit sensor->busyChanged();
}

void SensorAccess::error(QSensor *sensor, int error) { sensor->d->setError(error); }

void SensorAccess::addDataRate(QSensor *sensor, int minimum, int maximum) {
    sensor->d->info.dataRates.append(qrange(minimum, maximum));
}

void SensorAccess::addOutputRange(QSensor *sensor, const qoutputrange &range) {
    sensor->d->info.outputRanges.append(range);
}

void SensorAccess::setDescription(QSensor *sensor, const QString &description) {
    sensor->d->info.description = description;
}

void SensorAccess::setAdjuster(QSensor *sensor, std::function<void(QList<qreal> &)> adjuster) {
    sensor->d->adjuster = std::move(adjuster);
}

QList<qreal> SensorAccess::lastValues(const QSensor *sensor) { return sensor->d->raw; }

} // namespace QtSensorsPrivate

/*!
    \class QSensor
    \inmodule NtSensors
    \brief The QSensor class reads one sensor of the machine.
    \since 6.0

    A QSensor reads a sensor of one type, such as \c QAccelerometer, at dataRate() times a second
    while it is active, and emits readingChanged() with each new value in reading(). Use a subclass,
    such as QAccelerometer, for typed access to the reading, or QSensor with the name of the type:

    \code
    QPressureSensor barometer;
    QObject::connect(&barometer, &QSensor::readingChanged, [&barometer] {
        qDebug() << barometer.reading()->pressure() << "Pa";
    });
    barometer.setDataRate(5);
    if (!barometer.start())
        qWarning() << "no barometer";
    \endcode

    Without an identifier, start() takes the default sensor of the type, the first that
    sensorsForType() lists. Set one with setIdentifier() to choose another.

    \differences
    Readings are taken by polling from the event loop of the thread the sensor lives in, at the
    data rate (10 a second when none is set); there is no buffering, and availableSensorsChanged()
    is never emitted. A sensor of a backend registered with QSensorManager is driven by its backend. Axes orientation, output ranges and the
    \c AlwaysOn feature are not supported; alwaysOn is only stored. error() is a negative
    \c errno value. The backends are Linux Industrial I/O and Zephyr; the sensors of Android are
    not read yet.
    \enddifferences

    \sa {Using Nt Sensors}
*/

/*!
    \enum QSensor::Feature

    This enum describes the optional features of a sensor, for isFeatureSupported().

    \value Buffering Readings are buffered by the backend. Not supported.
    \value AlwaysOn The sensor keeps running when the screen is off. Not supported.
    \value GeoValues A magnetometer reports the geomagnetic field. Not supported.
    \value FieldOfView A light sensor reports its field of view. Not supported.
    \value AccelerationMode An accelerometer can leave out gravity or report only gravity. Not supported.
    \value SkipDuplicates Readings equal to the last one are dropped. Supported.
    \value AxesOrientation The axes follow the orientation of the screen. Not supported.
    \value PressureSensorTemperature A pressure sensor reports its temperature. Supported where
           the device measures it.
    \omitvalue Reserved
*/

/*!
    Constructs a sensor of \a type, such as \c QAccelerometer, with the given \a parent. Its
    reading is of the class that goes with the type, or a plain QSensorReading.
*/
QSensor::QSensor(const QByteArray &type, QObject *parent)
    : QSensor(type, QtSensorsPrivate::createReading(type, nullptr), parent) {}

/*!
    Constructs a sensor of \a type that reports into \a reading, which it takes ownership of, with
    the given \a parent. Subclasses such as QAccelerometer use it.
*/
QSensor::QSensor(const QByteArray &type, QSensorReading *reading, QObject *parent)
    : QObject(parent), d(std::make_unique<QSensorPrivate>(this, type, reading)) {}

/*!
    Destroys the sensor, stopping it and removing its filters.
*/
QSensor::~QSensor() {
    stop();
    for (QSensorFilter *filter : d->filters)
        filter->setSensor(nullptr);
    delete d->backend;
}

/*!
    Returns the identifier of the sensor to read, or an empty array before one is set or chosen.

    \sa setIdentifier(), sensorsForType()
*/
QByteArray QSensor::identifier() const { return d->identifier; }

/*!
    Sets the sensor to read to \a identifier, one of those that sensorsForType() lists. It can be
    changed only before the sensor is connected to the backend.
*/
void QSensor::setIdentifier(const QByteArray &identifier) {
    if (d->connected || identifier == d->identifier)
        return;
    d->identifier = identifier;
    emit identifierChanged();
}

/*!
    Returns the type of the sensor, such as \c QAccelerometer.
*/
QByteArray QSensor::type() const { return d->type; }

/*!
    Finds the sensor to read: the one identifier() names, or the default sensor of the type, which
    then becomes the identifier. Returns \c true if there is one. start() calls it.
*/
bool QSensor::connectToBackend() {
    if (d->connected)
        return true;
    // A backend registered with QSensorManager comes first.
    const QList<QByteArray> registered = QtSensorsPrivate::registeredIdentifiers(d->type);
    QByteArray wanted = d->identifier;
    if (wanted.isEmpty() && !registered.isEmpty())
        wanted = registered.first();
    if (!wanted.isEmpty() && registered.contains(wanted)) {
        d->info = QtSensorsPrivate::SensorInfo();
        d->info.identifier = wanted;
        d->info.type = d->type;
        d->info.dataRates.clear(); // the backend adds its own
        d->backend = QtSensorsPrivate::createRegisteredBackend(this, wanted);
        if (!d->backend)
            return false;
    } else {
        QtSensorsPrivate::SensorBackend *backend = QtSensorsPrivate::sensorBackend();
        if (!backend)
            return false;
        if (wanted.isEmpty())
            wanted = defaultSensorForType(d->type);
        for (const QtSensorsPrivate::SensorInfo &info : backend->sensors()) {
            if (info.type == d->type && info.identifier == wanted) {
                d->info = info;
                break;
            }
        }
        if (d->info.identifier.isEmpty()) {
            // A backend may open a device it does not list, such as a Zephyr device named by its devicetree node.
            if (d->identifier.isEmpty())
                return false;
            d->info.identifier = d->identifier;
            d->info.type = d->type;
        }
    }
    if (d->identifier != d->info.identifier) {
        d->identifier = d->info.identifier;
        emit identifierChanged();
    }
    d->connected = true;
    return true;
}

/*!
    Returns \c true once connectToBackend() has found the sensor to read.
*/
bool QSensor::isConnectedToBackend() const { return d->connected; }

/*!
    Returns \c true if the sensor is in use by another program. Only a QSensorBackend can tell, with
    QSensorBackend::sensorBusy(); the sensors that Nt reads itself are never busy.
*/
bool QSensor::isBusy() const { return d->busy; }

/*!
    Starts the sensor if \a active is \c true, or stops it otherwise.

    \sa start(), stop()
*/
void QSensor::setActive(bool active) {
    if (active)
        start();
    else
        stop();
}

/*!
    Returns \c true while the sensor is being read.
*/
bool QSensor::isActive() const { return d->active; }

/*!
    Returns \c true if the sensor should keep running when the screen is off. The setting is only
    stored.
*/
bool QSensor::isAlwaysOn() const { return d->alwaysOn; }

/*!
    Sets whether the sensor should keep running when the screen is off to \a alwaysOn. The setting
    is only stored.
*/
void QSensor::setAlwaysOn(bool alwaysOn) {
    if (d->alwaysOn == alwaysOn)
        return;
    d->alwaysOn = alwaysOn;
    emit alwaysOnChanged();
}

/*!
    Returns \c true if a reading equal to the last one is dropped.
*/
bool QSensor::skipDuplicates() const { return d->skipDuplicates; }

/*!
    Sets whether a reading equal to the last one is dropped to \a skipDuplicates.
*/
void QSensor::setSkipDuplicates(bool skipDuplicates) {
    if (d->skipDuplicates == skipDuplicates)
        return;
    d->skipDuplicates = skipDuplicates;
    emit skipDuplicatesChanged(skipDuplicates);
}

/*!
    Returns the ranges of data rates, in readings a second, that the sensor can be read at. It is
    empty until the sensor is connected to the backend.
*/
qrangelist QSensor::availableDataRates() const { return d->connected ? d->info.dataRates : qrangelist(); }

/*!
    Returns the number of readings a second, or 0 for the default of 10.
*/
int QSensor::dataRate() const { return d->dataRate; }

/*!
    Sets the number of readings a second to \a rate; 0 asks for the default of 10. A running
    sensor changes its rate at once.
*/
void QSensor::setDataRate(int rate) {
    if (rate < 0 || rate == d->dataRate)
        return;
    d->dataRate = rate;
    if (d->timer.isActive())
        d->timer.setInterval(d->interval());
    emit dataRateChanged();
}

/*!
    Returns the ranges the sensor can measure in. Nt reports none.
*/
qoutputrangelist QSensor::outputRanges() const { return d->info.outputRanges; }

/*!
    Returns the index of the output range in use, or -1 for the default.
*/
int QSensor::outputRange() const { return d->outputRange; }

/*!
    Chooses the output range at \a index. Nt only stores it.
*/
void QSensor::setOutputRange(int index) { d->outputRange = index; }

/*!
    Returns a description of the sensor, such as the name of its device, once it is connected to
    the backend.
*/
QString QSensor::description() const { return d->info.description; }

/*!
    Returns the last error of the sensor, a negative \c errno value, or 0.

    \sa sensorError()
*/
int QSensor::error() const { return d->error; }

/*!
    Adds \a filter, which the sensor does not take ownership of, to run on each reading before it
    is reported.

    \sa QSensorFilter
*/
void QSensor::addFilter(QSensorFilter *filter) {
    if (!filter || d->filters.contains(filter))
        return;
    if (filter->m_sensor)
        filter->m_sensor->removeFilter(filter);
    filter->setSensor(this);
    d->filters.append(filter);
}

/*!
    Removes \a filter.
*/
void QSensor::removeFilter(QSensorFilter *filter) {
    if (d->filters.removeAll(filter) > 0)
        filter->setSensor(nullptr);
}

/*!
    Returns the filters, in the order they run.
*/
QList<QSensorFilter *> QSensor::filters() const { return d->filters; }

/*!
    Returns the reading that the sensor updates. The sensor keeps ownership of it.
*/
QSensorReading *QSensor::reading() const { return d->reading; }

/*!
    Returns the types that the machine has at least one sensor of.
*/
QList<QByteArray> QSensor::sensorTypes() {
    QList<QByteArray> types = QtSensorsPrivate::registeredTypes();
    if (QtSensorsPrivate::SensorBackend *backend = QtSensorsPrivate::sensorBackend()) {
        for (const QtSensorsPrivate::SensorInfo &info : backend->sensors()) {
            if (!types.contains(info.type))
                types.append(info.type);
        }
    }
    return types;
}

/*!
    Returns the identifiers of the sensors of \a type, the default one first: the backends
    registered with QSensorManager, then the sensors that Nt reads itself.
*/
QList<QByteArray> QSensor::sensorsForType(const QByteArray &type) {
    QList<QByteArray> ids;
    if (QtSensorsPrivate::SensorBackend *backend = QtSensorsPrivate::sensorBackend()) {
        for (const QtSensorsPrivate::SensorInfo &info : backend->sensors()) {
            if (info.type != type || ids.contains(info.identifier))
                continue;
            if (info.isDefault)
                ids.prepend(info.identifier);
            else
                ids.append(info.identifier);
        }
    }
    // The backends registered with QSensorManager come first, the default of them at the front.
    const QList<QByteArray> registered = QtSensorsPrivate::registeredIdentifiers(type);
    for (auto it = registered.crbegin(); it != registered.crend(); ++it) {
        ids.removeAll(*it);
        ids.prepend(*it);
    }
    return ids;
}

/*!
    Returns the identifier of the default sensor of \a type, or an empty array if there is none.
*/
QByteArray QSensor::defaultSensorForType(const QByteArray &type) {
    const QList<QByteArray> ids = sensorsForType(type);
    return ids.isEmpty() ? QByteArray() : ids.first();
}

/*!
    Returns \c true if the sensor supports \a feature.
*/
bool QSensor::isFeatureSupported(QSensor::Feature feature) const {
    if (d->backend)
        return d->backend->isFeatureSupported(feature);
    switch (feature) {
    case SkipDuplicates:
        return true;
    case PressureSensorTemperature:
        return d->type == "QPressureSensor";
    default:
        return false;
    }
}

/*!
    Connects to the backend if needed, opens the sensor and starts reading it. Returns \c true if
    the sensor runs; otherwise sets error() and emits sensorError().

    \sa stop(), isActive()
*/
bool QSensor::start() {
    if (d->active)
        return true;
    if (!connectToBackend()) {
        d->setError(-ENODEV);
        return false;
    }
    d->error = 0;
    d->haveLast = false;
    if (d->backend) {
        // The backend reports with newReadingAvailable(), and with sensorStopped() if it cannot start.
        d->setActive(true);
        d->backend->start();
        return d->active;
    }
    int err = 0;
    d->device = QtSensorsPrivate::sensorBackend()->open(d->identifier, d->type, &err);
    if (!d->device) {
        d->setError(err < 0 ? err : -ENODEV);
        return false;
    }
    d->timer.start(d->interval());
    d->setActive(true);
    // The first reading comes at once rather than a period later.
    QTimer::singleShot(0, this, [this] {
        if (d->timer.isActive())
            d->sample();
    });
    return true;
}

/*!
    Stops reading the sensor and closes it.

    \sa start()
*/
void QSensor::stop() {
    if (!d->active)
        return;
    if (d->backend) {
        d->backend->stop();
    } else {
        d->timer.stop();
        d->device.reset();
    }
    d->setActive(false);
}

#include "moc_qsensor.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
