// MIT License - see LICENSE
//
// The typed sensors and readings of Nt Sensors, and the glue that makes the reading of a type and fills it in.
#include <QtSensors/qaccelerometer.h>
#include <QtSensors/qambienttemperaturesensor.h>
#include <QtSensors/qgyroscope.h>
#include <QtSensors/qhumiditysensor.h>
#include <QtSensors/qlightsensor.h>
#include <QtSensors/qmagnetometer.h>
#include <QtSensors/qpressuresensor.h>

#include "qsensorbackend_p.h"

namespace QtSensorsPrivate {

namespace {

// The reading of a type that Nt does not know: only a timestamp.
class PlainReading : public QSensorReading {
public:
    explicit PlainReading(QObject *parent) : QSensorReading(parent) {}
};

} // namespace

QSensorReading *createReading(const QByteArray &type, QObject *parent) {
    if (type == QAccelerometer::sensorType)
        return new QAccelerometerReading(parent);
    if (type == QGyroscope::sensorType)
        return new QGyroscopeReading(parent);
    if (type == QMagnetometer::sensorType)
        return new QMagnetometerReading(parent);
    if (type == QPressureSensor::sensorType)
        return new QPressureReading(parent);
    if (type == QAmbientTemperatureSensor::sensorType)
        return new QAmbientTemperatureReading(parent);
    if (type == QHumiditySensor::sensorType)
        return new QHumidityReading(parent);
    if (type == QLightSensor::sensorType)
        return new QLightReading(parent);
    return new PlainReading(parent);
}

void setReadingValues(QSensorReading *reading, const QList<qreal> &values) {
    const auto at = [&values](qsizetype i) { return i < values.size() ? values.at(i) : qreal(0); };
    if (auto *r = qobject_cast<QAccelerometerReading *>(reading)) {
        r->setX(at(0));
        r->setY(at(1));
        r->setZ(at(2));
        return;
    }
    if (auto *r = qobject_cast<QGyroscopeReading *>(reading)) {
        r->setX(at(0));
        r->setY(at(1));
        r->setZ(at(2));
        return;
    }
    if (auto *r = qobject_cast<QMagnetometerReading *>(reading)) {
        r->setX(at(0));
        r->setY(at(1));
        r->setZ(at(2));
        r->setCalibrationLevel(at(3));
        return;
    }
    if (auto *r = qobject_cast<QPressureReading *>(reading)) {
        r->setPressure(at(0));
        r->setTemperature(at(1));
        return;
    }
    if (auto *r = qobject_cast<QAmbientTemperatureReading *>(reading)) {
        r->setTemperature(at(0));
        return;
    }
    if (auto *r = qobject_cast<QHumidityReading *>(reading)) {
        r->setRelativeHumidity(at(0));
        r->setAbsoluteHumidity(at(1));
        return;
    }
    if (auto *r = qobject_cast<QLightReading *>(reading)) {
        r->setLux(at(0));
        return;
    }
}

} // namespace QtSensorsPrivate

/*!
    \class QAccelerometerReading
    \inmodule NtSensors
    \brief The QAccelerometerReading class holds a reading of an accelerometer.
    \since 6.0

    An accelerometer measures acceleration along three axes, including that of gravity, in metres a second squared.

    \differences
    None.
    \enddifferences

    \sa QAccelerometer, QAccelerometerFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QAccelerometerReading::QAccelerometerReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAccelerometerReading::~QAccelerometerReading() = default;

/*!
    Returns the x value. The acceleration along the x axis, in metres a second squared (m/s²).

    \sa setX()
*/
qreal QAccelerometerReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QAccelerometerReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The acceleration along the y axis, in m/s².

    \sa setY()
*/
qreal QAccelerometerReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QAccelerometerReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The acceleration along the z axis, in m/s². A device lying flat and still reads about +9.8.

    \sa setZ()
*/
qreal QAccelerometerReading::z() const { return m_z; }

/*!
    Sets the z value to \a z.
*/
void QAccelerometerReading::setZ(qreal z) { m_z = z; }

/*!
    \class QAccelerometerFilter
    \inmodule NtSensors
    \brief The QAccelerometerFilter class is a filter for the readings of QAccelerometer.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QAccelerometerFilter::filter(QAccelerometerReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QAccelerometerFilter::filter(QSensorReading *reading) { return filter(static_cast<QAccelerometerReading *>(reading)); }

/*!
    \class QAccelerometer
    \inmodule NtSensors
    \brief The QAccelerometer class reads an accelerometer.
    \since 6.0

    An accelerometer measures acceleration along three axes, including that of gravity, in metres a second squared. Each value is reported in reading() with readingChanged():

    \code
    QAccelerometer sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->x();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. Only the combined acceleration is reported; setAccelerationMode() is stored.
    \enddifferences

    \sa QAccelerometerReading, {Using Nt Sensors}
*/

/*!
    \variable QAccelerometer::sensorType

    The type of the sensor, \c QAccelerometer, as QSensor::type() returns it.
*/
const char *const QAccelerometer::sensorType = "QAccelerometer";

/*!
    Constructs a sensor with the given \a parent.
*/
QAccelerometer::QAccelerometer(QObject *parent) : QSensor(sensorType, new QAccelerometerReading, parent) {}

/*!
    Destroys the sensor.
*/
QAccelerometer::~QAccelerometer() = default;

/*!
    Returns the reading that the sensor updates.
*/
QAccelerometerReading *QAccelerometer::reading() const { return static_cast<QAccelerometerReading *>(QSensor::reading()); }

/*!
    \enum QAccelerometer::AccelerationMode

    This enum describes which acceleration is reported.

    \value Combined Gravity and the motion of the device together, as the sensor measures them.
    \value Gravity Only gravity. Not supported: the readings are as for \c Combined.
    \value User Only the motion of the device. Not supported: the readings are as for \c Combined.
*/

/*!
    Returns which acceleration is reported. It is \c Combined until set.
*/
QAccelerometer::AccelerationMode QAccelerometer::accelerationMode() const { return m_accelerationMode; }

/*!
    Sets which acceleration is reported to \a accelerationMode. Nt reads the sensor as it is, so
    only \c Combined changes nothing; the mode is stored.
*/
void QAccelerometer::setAccelerationMode(AccelerationMode accelerationMode) {
    if (m_accelerationMode == accelerationMode)
        return;
    m_accelerationMode = accelerationMode;
    emit accelerationModeChanged(accelerationMode);
}

/*!
    \class QGyroscopeReading
    \inmodule NtSensors
    \brief The QGyroscopeReading class holds a reading of a gyroscope.
    \since 6.0

    A gyroscope measures how fast the device turns around three axes, in degrees a second.

    \differences
    None.
    \enddifferences

    \sa QGyroscope, QGyroscopeFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QGyroscopeReading::QGyroscopeReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QGyroscopeReading::~QGyroscopeReading() = default;

/*!
    Returns the x value. The angular velocity around the x axis, in degrees a second.

    \sa setX()
*/
qreal QGyroscopeReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QGyroscopeReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The angular velocity around the y axis, in degrees a second.

    \sa setY()
*/
qreal QGyroscopeReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QGyroscopeReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The angular velocity around the z axis, in degrees a second.

    \sa setZ()
*/
qreal QGyroscopeReading::z() const { return m_z; }

/*!
    Sets the z value to \a z.
*/
void QGyroscopeReading::setZ(qreal z) { m_z = z; }

/*!
    \class QGyroscopeFilter
    \inmodule NtSensors
    \brief The QGyroscopeFilter class is a filter for the readings of QGyroscope.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QGyroscopeFilter::filter(QGyroscopeReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QGyroscopeFilter::filter(QSensorReading *reading) { return filter(static_cast<QGyroscopeReading *>(reading)); }

/*!
    \class QGyroscope
    \inmodule NtSensors
    \brief The QGyroscope class reads a gyroscope.
    \since 6.0

    A gyroscope measures how fast the device turns around three axes, in degrees a second. Each value is reported in reading() with readingChanged():

    \code
    QGyroscope sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->x();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QGyroscopeReading, {Using Nt Sensors}
*/

/*!
    \variable QGyroscope::sensorType

    The type of the sensor, \c QGyroscope, as QSensor::type() returns it.
*/
const char *const QGyroscope::sensorType = "QGyroscope";

/*!
    Constructs a sensor with the given \a parent.
*/
QGyroscope::QGyroscope(QObject *parent) : QSensor(sensorType, new QGyroscopeReading, parent) {}

/*!
    Destroys the sensor.
*/
QGyroscope::~QGyroscope() = default;

/*!
    Returns the reading that the sensor updates.
*/
QGyroscopeReading *QGyroscope::reading() const { return static_cast<QGyroscopeReading *>(QSensor::reading()); }

/*!
    \class QMagnetometerReading
    \inmodule NtSensors
    \brief The QMagnetometerReading class holds a reading of a magnetometer.
    \since 6.0

    A magnetometer measures the magnetic field along three axes, in tesla. The field of the Earth is about 25 to 65 microtesla.

    \differences
    None.
    \enddifferences

    \sa QMagnetometer, QMagnetometerFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QMagnetometerReading::QMagnetometerReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QMagnetometerReading::~QMagnetometerReading() = default;

/*!
    Returns the x value. The magnetic flux density along the x axis, in tesla.

    \sa setX()
*/
qreal QMagnetometerReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QMagnetometerReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The magnetic flux density along the y axis, in tesla.

    \sa setY()
*/
qreal QMagnetometerReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QMagnetometerReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The magnetic flux density along the z axis, in tesla.

    \sa setZ()
*/
qreal QMagnetometerReading::z() const { return m_z; }

/*!
    Sets the z value to \a z.
*/
void QMagnetometerReading::setZ(qreal z) { m_z = z; }

/*!
    Returns the calibrationLevel value. How well the sensor is calibrated, from 0 to 1. Nt cannot tell, and reports 1.

    \sa setCalibrationLevel()
*/
qreal QMagnetometerReading::calibrationLevel() const { return m_calibrationLevel; }

/*!
    Sets the calibrationLevel value to \a calibrationLevel.
*/
void QMagnetometerReading::setCalibrationLevel(qreal calibrationLevel) { m_calibrationLevel = calibrationLevel; }

/*!
    \class QMagnetometerFilter
    \inmodule NtSensors
    \brief The QMagnetometerFilter class is a filter for the readings of QMagnetometer.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QMagnetometerFilter::filter(QMagnetometerReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QMagnetometerFilter::filter(QSensorReading *reading) { return filter(static_cast<QMagnetometerReading *>(reading)); }

/*!
    \class QMagnetometer
    \inmodule NtSensors
    \brief The QMagnetometer class reads a magnetometer.
    \since 6.0

    A magnetometer measures the magnetic field along three axes, in tesla. The field of the Earth is about 25 to 65 microtesla. Each value is reported in reading() with readingChanged():

    \code
    QMagnetometer sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->x();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. The raw field is reported; returnGeoValues is stored, and calibrationLevel is always 1.
    \enddifferences

    \sa QMagnetometerReading, {Using Nt Sensors}
*/

/*!
    \variable QMagnetometer::sensorType

    The type of the sensor, \c QMagnetometer, as QSensor::type() returns it.
*/
const char *const QMagnetometer::sensorType = "QMagnetometer";

/*!
    Constructs a sensor with the given \a parent.
*/
QMagnetometer::QMagnetometer(QObject *parent) : QSensor(sensorType, new QMagnetometerReading, parent) {}

/*!
    Destroys the sensor.
*/
QMagnetometer::~QMagnetometer() = default;

/*!
    Returns the reading that the sensor updates.
*/
QMagnetometerReading *QMagnetometer::reading() const { return static_cast<QMagnetometerReading *>(QSensor::reading()); }

/*!
    Returns \c true if the geomagnetic field alone should be reported, without the local
    interference. It is \c false until set.
*/
bool QMagnetometer::returnGeoValues() const { return m_returnGeoValues; }

/*!
    Sets whether the geomagnetic field alone should be reported to \a returnGeoValues. Nt reports
    the raw field either way; the setting is stored.
*/
void QMagnetometer::setReturnGeoValues(bool returnGeoValues) {
    if (m_returnGeoValues == returnGeoValues)
        return;
    m_returnGeoValues = returnGeoValues;
    emit returnGeoValuesChanged(returnGeoValues);
}

/*!
    \class QPressureReading
    \inmodule NtSensors
    \brief The QPressureReading class holds a reading of a pressure sensor.
    \since 6.0

    A pressure sensor, or barometer, measures atmospheric pressure in pascals.

    \differences
    None.
    \enddifferences

    \sa QPressureSensor, QPressureFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QPressureReading::QPressureReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QPressureReading::~QPressureReading() = default;

/*!
    Returns the pressure value. The atmospheric pressure, in pascals. At sea level it is about 101325 Pa.

    \sa setPressure()
*/
qreal QPressureReading::pressure() const { return m_pressure; }

/*!
    Sets the pressure value to \a pressure.
*/
void QPressureReading::setPressure(qreal pressure) { m_pressure = pressure; }

/*!
    Returns the temperature value. The temperature of the sensor, in degrees Celsius, where the device measures it; otherwise 0.

    \sa setTemperature()
*/
qreal QPressureReading::temperature() const { return m_temperature; }

/*!
    Sets the temperature value to \a temperature.
*/
void QPressureReading::setTemperature(qreal temperature) { m_temperature = temperature; }

/*!
    \class QPressureFilter
    \inmodule NtSensors
    \brief The QPressureFilter class is a filter for the readings of QPressureSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QPressureFilter::filter(QPressureReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QPressureFilter::filter(QSensorReading *reading) { return filter(static_cast<QPressureReading *>(reading)); }

/*!
    \class QPressureSensor
    \inmodule NtSensors
    \brief The QPressureSensor class reads a pressure sensor.
    \since 6.0

    A pressure sensor, or barometer, measures atmospheric pressure in pascals. Each value is reported in reading() with readingChanged():

    \code
    QPressureSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->pressure();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QPressureReading, {Using Nt Sensors}
*/

/*!
    \variable QPressureSensor::sensorType

    The type of the sensor, \c QPressureSensor, as QSensor::type() returns it.
*/
const char *const QPressureSensor::sensorType = "QPressureSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QPressureSensor::QPressureSensor(QObject *parent) : QSensor(sensorType, new QPressureReading, parent) {}

/*!
    Destroys the sensor.
*/
QPressureSensor::~QPressureSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QPressureReading *QPressureSensor::reading() const { return static_cast<QPressureReading *>(QSensor::reading()); }

/*!
    \class QAmbientTemperatureReading
    \inmodule NtSensors
    \brief The QAmbientTemperatureReading class holds a reading of a temperature sensor.
    \since 6.0

    An ambient temperature sensor measures the temperature of the air around it, in degrees Celsius. On Zephyr the temperature of the chip itself is reported when there is no ambient sensor.

    \differences
    None.
    \enddifferences

    \sa QAmbientTemperatureSensor, QAmbientTemperatureFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QAmbientTemperatureReading::QAmbientTemperatureReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAmbientTemperatureReading::~QAmbientTemperatureReading() = default;

/*!
    Returns the temperature value. The temperature, in degrees Celsius.

    \sa setTemperature()
*/
qreal QAmbientTemperatureReading::temperature() const { return m_temperature; }

/*!
    Sets the temperature value to \a temperature.
*/
void QAmbientTemperatureReading::setTemperature(qreal temperature) { m_temperature = temperature; }

/*!
    \class QAmbientTemperatureFilter
    \inmodule NtSensors
    \brief The QAmbientTemperatureFilter class is a filter for the readings of QAmbientTemperatureSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QAmbientTemperatureFilter::filter(QAmbientTemperatureReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QAmbientTemperatureFilter::filter(QSensorReading *reading) { return filter(static_cast<QAmbientTemperatureReading *>(reading)); }

/*!
    \class QAmbientTemperatureSensor
    \inmodule NtSensors
    \brief The QAmbientTemperatureSensor class reads a temperature sensor.
    \since 6.0

    An ambient temperature sensor measures the temperature of the air around it, in degrees Celsius. On Zephyr the temperature of the chip itself is reported when there is no ambient sensor. Each value is reported in reading() with readingChanged():

    \code
    QAmbientTemperatureSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->temperature();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QAmbientTemperatureReading, {Using Nt Sensors}
*/

/*!
    \variable QAmbientTemperatureSensor::sensorType

    The type of the sensor, \c QAmbientTemperatureSensor, as QSensor::type() returns it.
*/
const char *const QAmbientTemperatureSensor::sensorType = "QAmbientTemperatureSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QAmbientTemperatureSensor::QAmbientTemperatureSensor(QObject *parent) : QSensor(sensorType, new QAmbientTemperatureReading, parent) {}

/*!
    Destroys the sensor.
*/
QAmbientTemperatureSensor::~QAmbientTemperatureSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QAmbientTemperatureReading *QAmbientTemperatureSensor::reading() const { return static_cast<QAmbientTemperatureReading *>(QSensor::reading()); }

/*!
    \class QHumidityReading
    \inmodule NtSensors
    \brief The QHumidityReading class holds a reading of a humidity sensor.
    \since 6.0

    A humidity sensor measures the relative humidity of the air, in percent.

    \differences
    None.
    \enddifferences

    \sa QHumiditySensor, QHumidityFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QHumidityReading::QHumidityReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QHumidityReading::~QHumidityReading() = default;

/*!
    Returns the relativeHumidity value. The relative humidity, in percent.

    \sa setRelativeHumidity()
*/
qreal QHumidityReading::relativeHumidity() const { return m_relativeHumidity; }

/*!
    Sets the relativeHumidity value to \a relativeHumidity.
*/
void QHumidityReading::setRelativeHumidity(qreal relativeHumidity) { m_relativeHumidity = relativeHumidity; }

/*!
    Returns the absoluteHumidity value. The absolute humidity, in grams of water a cubic metre, where the device measures it; otherwise 0.

    \sa setAbsoluteHumidity()
*/
qreal QHumidityReading::absoluteHumidity() const { return m_absoluteHumidity; }

/*!
    Sets the absoluteHumidity value to \a absoluteHumidity.
*/
void QHumidityReading::setAbsoluteHumidity(qreal absoluteHumidity) { m_absoluteHumidity = absoluteHumidity; }

/*!
    \class QHumidityFilter
    \inmodule NtSensors
    \brief The QHumidityFilter class is a filter for the readings of QHumiditySensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QHumidityFilter::filter(QHumidityReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QHumidityFilter::filter(QSensorReading *reading) { return filter(static_cast<QHumidityReading *>(reading)); }

/*!
    \class QHumiditySensor
    \inmodule NtSensors
    \brief The QHumiditySensor class reads a humidity sensor.
    \since 6.0

    A humidity sensor measures the relative humidity of the air, in percent. Each value is reported in reading() with readingChanged():

    \code
    QHumiditySensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->relativeHumidity();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QHumidityReading, {Using Nt Sensors}
*/

/*!
    \variable QHumiditySensor::sensorType

    The type of the sensor, \c QHumiditySensor, as QSensor::type() returns it.
*/
const char *const QHumiditySensor::sensorType = "QHumiditySensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QHumiditySensor::QHumiditySensor(QObject *parent) : QSensor(sensorType, new QHumidityReading, parent) {}

/*!
    Destroys the sensor.
*/
QHumiditySensor::~QHumiditySensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QHumidityReading *QHumiditySensor::reading() const { return static_cast<QHumidityReading *>(QSensor::reading()); }

/*!
    \class QLightReading
    \inmodule NtSensors
    \brief The QLightReading class holds a reading of a light sensor.
    \since 6.0

    A light sensor measures illuminance, in lux.

    \differences
    None.
    \enddifferences

    \sa QLightSensor, QLightFilter
*/

/*!
    Constructs a reading with zero values and the given \a parent.
*/
QLightReading::QLightReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QLightReading::~QLightReading() = default;

/*!
    Returns the lux value. The illuminance, in lux.

    \sa setLux()
*/
qreal QLightReading::lux() const { return m_lux; }

/*!
    Sets the lux value to \a lux.
*/
void QLightReading::setLux(qreal lux) { m_lux = lux; }

/*!
    \class QLightFilter
    \inmodule NtSensors
    \brief The QLightFilter class is a filter for the readings of QLightSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QLightFilter::filter(QLightReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QLightFilter::filter(QSensorReading *reading) { return filter(static_cast<QLightReading *>(reading)); }

/*!
    \class QLightSensor
    \inmodule NtSensors
    \brief The QLightSensor class reads a light sensor.
    \since 6.0

    A light sensor measures illuminance, in lux. Each value is reported in reading() with readingChanged():

    \code
    QLightSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->lux();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. The field of view is not known to the backends and reads 0.
    \enddifferences

    \sa QLightReading, {Using Nt Sensors}
*/

/*!
    \variable QLightSensor::sensorType

    The type of the sensor, \c QLightSensor, as QSensor::type() returns it.
*/
const char *const QLightSensor::sensorType = "QLightSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QLightSensor::QLightSensor(QObject *parent) : QSensor(sensorType, new QLightReading, parent) {}

/*!
    Destroys the sensor.
*/
QLightSensor::~QLightSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QLightReading *QLightSensor::reading() const { return static_cast<QLightReading *>(QSensor::reading()); }

/*!
    Returns the angle, in degrees, that the sensor sees light from, or 0 if it is not known.
*/
qreal QLightSensor::fieldOfView() const { return m_fieldOfView; }

/*!
    Sets the field of view to \a fieldOfView degrees; for a backend that knows it.
*/
void QLightSensor::setFieldOfView(qreal fieldOfView) {
    if (m_fieldOfView == fieldOfView)
        return;
    m_fieldOfView = fieldOfView;
    emit fieldOfViewChanged(fieldOfView);
}

#include "moc_qaccelerometer.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qambienttemperaturesensor.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qgyroscope.cpp"         // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qhumiditysensor.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qlightsensor.cpp"       // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qmagnetometer.cpp"      // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qpressuresensor.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
