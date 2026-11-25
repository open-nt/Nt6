// MIT License - see LICENSE
//
// The typed sensors and readings of Nt Sensors, and the glue that makes the reading of a type and moves values in and
// out of it. Generated from one table, so that every type follows the same pattern.
#include <QtSensors/qaccelerometer.h>
#include <QtSensors/qaltimeter.h>
#include <QtSensors/qambientlightsensor.h>
#include <QtSensors/qambienttemperaturesensor.h>
#include <QtSensors/qcompass.h>
#include <QtSensors/qgyroscope.h>
#include <QtSensors/qholstersensor.h>
#include <QtSensors/qhumiditysensor.h>
#include <QtSensors/qirproximitysensor.h>
#include <QtSensors/qlidsensor.h>
#include <QtSensors/qlightsensor.h>
#include <QtSensors/qmagnetometer.h>
#include <QtSensors/qorientationsensor.h>
#include <QtSensors/qpressuresensor.h>
#include <QtSensors/qproximitysensor.h>
#include <QtSensors/qrotationsensor.h>
#include <QtSensors/qtapsensor.h>
#include <QtSensors/qtiltsensor.h>

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
    if (type == QAltimeter::sensorType)
        return new QAltimeterReading(parent);
    if (type == QAmbientLightSensor::sensorType)
        return new QAmbientLightReading(parent);
    if (type == QAmbientTemperatureSensor::sensorType)
        return new QAmbientTemperatureReading(parent);
    if (type == QCompass::sensorType)
        return new QCompassReading(parent);
    if (type == QGyroscope::sensorType)
        return new QGyroscopeReading(parent);
    if (type == QHolsterSensor::sensorType)
        return new QHolsterReading(parent);
    if (type == QHumiditySensor::sensorType)
        return new QHumidityReading(parent);
    if (type == QIRProximitySensor::sensorType)
        return new QIRProximityReading(parent);
    if (type == QLidSensor::sensorType)
        return new QLidReading(parent);
    if (type == QLightSensor::sensorType)
        return new QLightReading(parent);
    if (type == QMagnetometer::sensorType)
        return new QMagnetometerReading(parent);
    if (type == QOrientationSensor::sensorType)
        return new QOrientationReading(parent);
    if (type == QPressureSensor::sensorType)
        return new QPressureReading(parent);
    if (type == QProximitySensor::sensorType)
        return new QProximityReading(parent);
    if (type == QRotationSensor::sensorType)
        return new QRotationReading(parent);
    if (type == QTapSensor::sensorType)
        return new QTapReading(parent);
    if (type == QTiltSensor::sensorType)
        return new QTiltReading(parent);
    return new PlainReading(parent);
}

void setReadingValues(QSensorReading *reading, const QList<qreal> &values) {
    const auto at = [&values](qsizetype i) { return i < values.size() ? values.at(i) : qreal(0); };
    if (auto *q = qobject_cast<QAccelerometerReading *>(reading)) {
        q->setX(at(0));
        q->setY(at(1));
        q->setZ(at(2));
        return;
    }
    if (auto *q = qobject_cast<QAltimeterReading *>(reading)) {
        q->setAltitude(at(0));
        return;
    }
    if (auto *q = qobject_cast<QAmbientLightReading *>(reading)) {
        q->setLightLevel(QAmbientLightReading::LightLevel(int(at(0))));
        return;
    }
    if (auto *q = qobject_cast<QAmbientTemperatureReading *>(reading)) {
        q->setTemperature(at(0));
        return;
    }
    if (auto *q = qobject_cast<QCompassReading *>(reading)) {
        q->setAzimuth(at(0));
        q->setCalibrationLevel(at(1));
        return;
    }
    if (auto *q = qobject_cast<QGyroscopeReading *>(reading)) {
        q->setX(at(0));
        q->setY(at(1));
        q->setZ(at(2));
        return;
    }
    if (auto *q = qobject_cast<QHolsterReading *>(reading)) {
        q->setHolstered(at(0) != 0);
        return;
    }
    if (auto *q = qobject_cast<QHumidityReading *>(reading)) {
        q->setRelativeHumidity(at(0));
        q->setAbsoluteHumidity(at(1));
        return;
    }
    if (auto *q = qobject_cast<QIRProximityReading *>(reading)) {
        q->setReflectance(at(0));
        return;
    }
    if (auto *q = qobject_cast<QLidReading *>(reading)) {
        q->setBackLidClosed(at(0) != 0);
        q->setFrontLidClosed(at(1) != 0);
        return;
    }
    if (auto *q = qobject_cast<QLightReading *>(reading)) {
        q->setLux(at(0));
        return;
    }
    if (auto *q = qobject_cast<QMagnetometerReading *>(reading)) {
        q->setX(at(0));
        q->setY(at(1));
        q->setZ(at(2));
        q->setCalibrationLevel(at(3));
        return;
    }
    if (auto *q = qobject_cast<QOrientationReading *>(reading)) {
        q->setOrientation(QOrientationReading::Orientation(int(at(0))));
        return;
    }
    if (auto *q = qobject_cast<QPressureReading *>(reading)) {
        q->setPressure(at(0));
        q->setTemperature(at(1));
        return;
    }
    if (auto *q = qobject_cast<QProximityReading *>(reading)) {
        q->setClose(at(0) != 0);
        return;
    }
    if (auto *q = qobject_cast<QRotationReading *>(reading)) {
        q->setFromEuler(at(0), at(1), at(2));
        return;
    }
    if (auto *q = qobject_cast<QTapReading *>(reading)) {
        q->setTapDirection(QTapReading::TapDirection(int(at(0))));
        q->setDoubleTap(at(1) != 0);
        return;
    }
    if (auto *q = qobject_cast<QTiltReading *>(reading)) {
        q->setYRotation(at(0));
        q->setXRotation(at(1));
        return;
    }
}

QList<qreal> readingValues(const QSensorReading *reading) {
    if (auto *q = qobject_cast<const QAccelerometerReading *>(reading))
        return {q->x(), q->y(), q->z()};
    if (auto *q = qobject_cast<const QAltimeterReading *>(reading))
        return {q->altitude()};
    if (auto *q = qobject_cast<const QAmbientLightReading *>(reading))
        return {qreal(q->lightLevel())};
    if (auto *q = qobject_cast<const QAmbientTemperatureReading *>(reading))
        return {q->temperature()};
    if (auto *q = qobject_cast<const QCompassReading *>(reading))
        return {q->azimuth(), q->calibrationLevel()};
    if (auto *q = qobject_cast<const QGyroscopeReading *>(reading))
        return {q->x(), q->y(), q->z()};
    if (auto *q = qobject_cast<const QHolsterReading *>(reading))
        return {q->holstered() ? 1.0 : 0.0};
    if (auto *q = qobject_cast<const QHumidityReading *>(reading))
        return {q->relativeHumidity(), q->absoluteHumidity()};
    if (auto *q = qobject_cast<const QIRProximityReading *>(reading))
        return {q->reflectance()};
    if (auto *q = qobject_cast<const QLidReading *>(reading))
        return {q->backLidClosed() ? 1.0 : 0.0, q->frontLidClosed() ? 1.0 : 0.0};
    if (auto *q = qobject_cast<const QLightReading *>(reading))
        return {q->lux()};
    if (auto *q = qobject_cast<const QMagnetometerReading *>(reading))
        return {q->x(), q->y(), q->z(), q->calibrationLevel()};
    if (auto *q = qobject_cast<const QOrientationReading *>(reading))
        return {qreal(q->orientation())};
    if (auto *q = qobject_cast<const QPressureReading *>(reading))
        return {q->pressure(), q->temperature()};
    if (auto *q = qobject_cast<const QProximityReading *>(reading))
        return {q->close() ? 1.0 : 0.0};
    if (auto *q = qobject_cast<const QRotationReading *>(reading))
        return {q->x(), q->y(), q->z()};
    if (auto *q = qobject_cast<const QTapReading *>(reading))
        return {qreal(q->tapDirection()), q->isDoubleTap() ? 1.0 : 0.0};
    if (auto *q = qobject_cast<const QTiltReading *>(reading))
        return {q->yRotation(), q->xRotation()};
    return {};
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
    Constructs a reading with the given \a parent.
*/
QAccelerometerReading::QAccelerometerReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAccelerometerReading::~QAccelerometerReading() = default;

/*!
    Returns the x value. The acceleration along the x axis, in metres a second squared (m/s²).
*/
qreal QAccelerometerReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QAccelerometerReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The acceleration along the y axis, in m/s².
*/
qreal QAccelerometerReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QAccelerometerReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The acceleration along the z axis, in m/s². A device lying flat and still reads about +9.8.
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
bool QAccelerometerFilter::filter(QSensorReading *reading) {
    return filter(static_cast<QAccelerometerReading *>(reading));
}

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
QAccelerometerReading *QAccelerometer::reading() const {
    return static_cast<QAccelerometerReading *>(QSensor::reading());
}

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
    \class QAltimeterReading
    \inmodule NtSensors
    \brief The QAltimeterReading class holds a reading of an altimeter.
    \since 6.0

    An altimeter reports the altitude above sea level, in metres. Nt computes it from a pressure sensor with the international barometric formula, h = 44330 (1 - (p / 101325)^(1/5.255)), for the standard atmosphere; weather moves it by tens of metres.

    \differences
    None.
    \enddifferences

    \sa QAltimeter, QAltimeterFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QAltimeterReading::QAltimeterReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAltimeterReading::~QAltimeterReading() = default;

/*!
    Returns the altitude value. The altitude, in metres above sea level.
*/
qreal QAltimeterReading::altitude() const { return m_altitude; }

/*!
    Sets the altitude value to \a altitude.
*/
void QAltimeterReading::setAltitude(qreal altitude) { m_altitude = altitude; }

/*!
    \class QAltimeterFilter
    \inmodule NtSensors
    \brief The QAltimeterFilter class is a filter for the readings of QAltimeter.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QAltimeterFilter::filter(QAltimeterReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QAltimeterFilter::filter(QSensorReading *reading) { return filter(static_cast<QAltimeterReading *>(reading)); }

/*!
    \class QAltimeter
    \inmodule NtSensors
    \brief The QAltimeter class reads an altimeter.
    \since 6.0

    An altimeter reports the altitude above sea level, in metres. Nt computes it from a pressure sensor with the international barometric formula, h = 44330 (1 - (p / 101325)^(1/5.255)), for the standard atmosphere; weather moves it by tens of metres. Each value is reported in reading() with readingChanged():

    \code
    QAltimeter sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->altitude();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QAltimeterReading, {Using Nt Sensors}
*/

/*!
    \variable QAltimeter::sensorType

    The type of the sensor, \c QAltimeter, as QSensor::type() returns it.
*/
const char *const QAltimeter::sensorType = "QAltimeter";

/*!
    Constructs a sensor with the given \a parent.
*/
QAltimeter::QAltimeter(QObject *parent) : QSensor(sensorType, new QAltimeterReading, parent) {}

/*!
    Destroys the sensor.
*/
QAltimeter::~QAltimeter() = default;

/*!
    Returns the reading that the sensor updates.
*/
QAltimeterReading *QAltimeter::reading() const { return static_cast<QAltimeterReading *>(QSensor::reading()); }

/*!
    \class QAmbientLightReading
    \inmodule NtSensors
    \brief The QAmbientLightReading class holds a reading of an ambient light sensor.
    \since 6.0

    An ambient light sensor reports how bright it is around the device, as a level. Nt computes the level from a light sensor: below 10 lux is dark, below 50 twilight, below 500 light, below 10000 bright, and from 10000 lux up sunny.

    \differences
    None.
    \enddifferences

    \sa QAmbientLightSensor, QAmbientLightFilter
*/

/*!
    \enum QAmbientLightReading::LightLevel

    This enum describes the values of the reading.

    \value Undefined The level is not known.
    \value Dark Darkness, below 10 lux.
    \value Twilight Twilight or a dim room, 10 to 50 lux.
    \value Light An ordinary room, 50 to 500 lux.
    \value Bright A bright room or an overcast day, 500 to 10000 lux.
    \value Sunny Daylight in the sun, 10000 lux and more.
*/

/*!
    Constructs a reading with the given \a parent.
*/
QAmbientLightReading::QAmbientLightReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAmbientLightReading::~QAmbientLightReading() = default;

/*!
    Returns the lightLevel value. The light level.
*/
QAmbientLightReading::LightLevel QAmbientLightReading::lightLevel() const { return m_lightLevel; }

/*!
    Sets the lightLevel value to \a lightLevel.
*/
void QAmbientLightReading::setLightLevel(LightLevel lightLevel) { m_lightLevel = lightLevel; }

/*!
    \class QAmbientLightFilter
    \inmodule NtSensors
    \brief The QAmbientLightFilter class is a filter for the readings of QAmbientLightSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QAmbientLightFilter::filter(QAmbientLightReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QAmbientLightFilter::filter(QSensorReading *reading) {
    return filter(static_cast<QAmbientLightReading *>(reading));
}

/*!
    \class QAmbientLightSensor
    \inmodule NtSensors
    \brief The QAmbientLightSensor class reads an ambient light sensor.
    \since 6.0

    An ambient light sensor reports how bright it is around the device, as a level. Nt computes the level from a light sensor: below 10 lux is dark, below 50 twilight, below 500 light, below 10000 bright, and from 10000 lux up sunny. Each value is reported in reading() with readingChanged():

    \code
    QAmbientLightSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->lightLevel();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QAmbientLightReading, {Using Nt Sensors}
*/

/*!
    \variable QAmbientLightSensor::sensorType

    The type of the sensor, \c QAmbientLightSensor, as QSensor::type() returns it.
*/
const char *const QAmbientLightSensor::sensorType = "QAmbientLightSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QAmbientLightSensor::QAmbientLightSensor(QObject *parent) : QSensor(sensorType, new QAmbientLightReading, parent) {}

/*!
    Destroys the sensor.
*/
QAmbientLightSensor::~QAmbientLightSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QAmbientLightReading *QAmbientLightSensor::reading() const {
    return static_cast<QAmbientLightReading *>(QSensor::reading());
}

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
    Constructs a reading with the given \a parent.
*/
QAmbientTemperatureReading::QAmbientTemperatureReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QAmbientTemperatureReading::~QAmbientTemperatureReading() = default;

/*!
    Returns the temperature value. The temperature, in degrees Celsius.
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
bool QAmbientTemperatureFilter::filter(QSensorReading *reading) {
    return filter(static_cast<QAmbientTemperatureReading *>(reading));
}

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
QAmbientTemperatureSensor::QAmbientTemperatureSensor(QObject *parent)
    : QSensor(sensorType, new QAmbientTemperatureReading, parent) {}

/*!
    Destroys the sensor.
*/
QAmbientTemperatureSensor::~QAmbientTemperatureSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QAmbientTemperatureReading *QAmbientTemperatureSensor::reading() const {
    return static_cast<QAmbientTemperatureReading *>(QSensor::reading());
}

/*!
    \class QCompassReading
    \inmodule NtSensors
    \brief The QCompassReading class holds a reading of a compass.
    \since 6.0

    A compass reports the heading of the top of the device from magnetic north, in degrees clockwise. Nt computes it from an accelerometer and a magnetometer, compensating for tilt; it is not corrected for declination.

    \differences
    None.
    \enddifferences

    \sa QCompass, QCompassFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QCompassReading::QCompassReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QCompassReading::~QCompassReading() = default;

/*!
    Returns the azimuth value. The heading, in degrees from magnetic north, from 0 to 360.
*/
qreal QCompassReading::azimuth() const { return m_azimuth; }

/*!
    Sets the azimuth value to \a azimuth.
*/
void QCompassReading::setAzimuth(qreal azimuth) { m_azimuth = azimuth; }

/*!
    Returns the calibrationLevel value. How well the compass is calibrated, from 0 to 1. Nt cannot tell, and reports 1.
*/
qreal QCompassReading::calibrationLevel() const { return m_calibrationLevel; }

/*!
    Sets the calibrationLevel value to \a calibrationLevel.
*/
void QCompassReading::setCalibrationLevel(qreal calibrationLevel) { m_calibrationLevel = calibrationLevel; }

/*!
    \class QCompassFilter
    \inmodule NtSensors
    \brief The QCompassFilter class is a filter for the readings of QCompass.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QCompassFilter::filter(QCompassReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QCompassFilter::filter(QSensorReading *reading) { return filter(static_cast<QCompassReading *>(reading)); }

/*!
    \class QCompass
    \inmodule NtSensors
    \brief The QCompass class reads a compass.
    \since 6.0

    A compass reports the heading of the top of the device from magnetic north, in degrees clockwise. Nt computes it from an accelerometer and a magnetometer, compensating for tilt; it is not corrected for declination. Each value is reported in reading() with readingChanged():

    \code
    QCompass sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->azimuth();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QCompassReading, {Using Nt Sensors}
*/

/*!
    \variable QCompass::sensorType

    The type of the sensor, \c QCompass, as QSensor::type() returns it.
*/
const char *const QCompass::sensorType = "QCompass";

/*!
    Constructs a sensor with the given \a parent.
*/
QCompass::QCompass(QObject *parent) : QSensor(sensorType, new QCompassReading, parent) {}

/*!
    Destroys the sensor.
*/
QCompass::~QCompass() = default;

/*!
    Returns the reading that the sensor updates.
*/
QCompassReading *QCompass::reading() const { return static_cast<QCompassReading *>(QSensor::reading()); }

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
    Constructs a reading with the given \a parent.
*/
QGyroscopeReading::QGyroscopeReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QGyroscopeReading::~QGyroscopeReading() = default;

/*!
    Returns the x value. The angular velocity around the x axis, in degrees a second.
*/
qreal QGyroscopeReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QGyroscopeReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The angular velocity around the y axis, in degrees a second.
*/
qreal QGyroscopeReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QGyroscopeReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The angular velocity around the z axis, in degrees a second.
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
    \class QHolsterReading
    \inmodule NtSensors
    \brief The QHolsterReading class holds a reading of a holster sensor.
    \since 6.0

    A holster sensor tells whether the device is in a holster. No backend of Nt has one; a QSensorBackend can provide it.

    \differences
    None.
    \enddifferences

    \sa QHolsterSensor, QHolsterFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QHolsterReading::QHolsterReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QHolsterReading::~QHolsterReading() = default;

/*!
    Returns the holstered value. \c true if the device is in a holster.
*/
bool QHolsterReading::holstered() const { return m_holstered; }

/*!
    Sets the holstered value to \a holstered.
*/
void QHolsterReading::setHolstered(bool holstered) { m_holstered = holstered; }

/*!
    \class QHolsterFilter
    \inmodule NtSensors
    \brief The QHolsterFilter class is a filter for the readings of QHolsterSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QHolsterFilter::filter(QHolsterReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QHolsterFilter::filter(QSensorReading *reading) { return filter(static_cast<QHolsterReading *>(reading)); }

/*!
    \class QHolsterSensor
    \inmodule NtSensors
    \brief The QHolsterSensor class reads a holster sensor.
    \since 6.0

    A holster sensor tells whether the device is in a holster. No backend of Nt has one; a QSensorBackend can provide it. Each value is reported in reading() with readingChanged():

    \code
    QHolsterSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->holstered();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QHolsterReading, {Using Nt Sensors}
*/

/*!
    \variable QHolsterSensor::sensorType

    The type of the sensor, \c QHolsterSensor, as QSensor::type() returns it.
*/
const char *const QHolsterSensor::sensorType = "QHolsterSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QHolsterSensor::QHolsterSensor(QObject *parent) : QSensor(sensorType, new QHolsterReading, parent) {}

/*!
    Destroys the sensor.
*/
QHolsterSensor::~QHolsterSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QHolsterReading *QHolsterSensor::reading() const { return static_cast<QHolsterReading *>(QSensor::reading()); }

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
    Constructs a reading with the given \a parent.
*/
QHumidityReading::QHumidityReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QHumidityReading::~QHumidityReading() = default;

/*!
    Returns the relativeHumidity value. The relative humidity, in percent.
*/
qreal QHumidityReading::relativeHumidity() const { return m_relativeHumidity; }

/*!
    Sets the relativeHumidity value to \a relativeHumidity.
*/
void QHumidityReading::setRelativeHumidity(qreal relativeHumidity) { m_relativeHumidity = relativeHumidity; }

/*!
    Returns the absoluteHumidity value. The absolute humidity, in grams of water a cubic metre, where the device measures it; otherwise 0.
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
    \class QIRProximityReading
    \inmodule NtSensors
    \brief The QIRProximityReading class holds a reading of an infrared proximity sensor.
    \since 6.0

    An infrared proximity sensor reports how much of its infrared light comes back, which rises as an object comes closer. On Linux it is the proximity channel of an Industrial I/O device, as its driver scales it.

    \differences
    None.
    \enddifferences

    \sa QIRProximitySensor, QIRProximityFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QIRProximityReading::QIRProximityReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QIRProximityReading::~QIRProximityReading() = default;

/*!
    Returns the reflectance value. The reflectance, from 0 for nothing to 1 for all of the light, as the driver scales it.
*/
qreal QIRProximityReading::reflectance() const { return m_reflectance; }

/*!
    Sets the reflectance value to \a reflectance.
*/
void QIRProximityReading::setReflectance(qreal reflectance) { m_reflectance = reflectance; }

/*!
    \class QIRProximityFilter
    \inmodule NtSensors
    \brief The QIRProximityFilter class is a filter for the readings of QIRProximitySensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QIRProximityFilter::filter(QIRProximityReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QIRProximityFilter::filter(QSensorReading *reading) { return filter(static_cast<QIRProximityReading *>(reading)); }

/*!
    \class QIRProximitySensor
    \inmodule NtSensors
    \brief The QIRProximitySensor class reads an infrared proximity sensor.
    \since 6.0

    An infrared proximity sensor reports how much of its infrared light comes back, which rises as an object comes closer. On Linux it is the proximity channel of an Industrial I/O device, as its driver scales it. Each value is reported in reading() with readingChanged():

    \code
    QIRProximitySensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->reflectance();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QIRProximityReading, {Using Nt Sensors}
*/

/*!
    \variable QIRProximitySensor::sensorType

    The type of the sensor, \c QIRProximitySensor, as QSensor::type() returns it.
*/
const char *const QIRProximitySensor::sensorType = "QIRProximitySensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QIRProximitySensor::QIRProximitySensor(QObject *parent) : QSensor(sensorType, new QIRProximityReading, parent) {}

/*!
    Destroys the sensor.
*/
QIRProximitySensor::~QIRProximitySensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QIRProximityReading *QIRProximitySensor::reading() const {
    return static_cast<QIRProximityReading *>(QSensor::reading());
}

/*!
    \class QLidReading
    \inmodule NtSensors
    \brief The QLidReading class holds a reading of a lid sensor.
    \since 6.0

    A lid sensor tells whether the lid of a laptop, or the cover of a tablet, is closed. On Linux the lid is that of the ACPI button under \c{/proc/acpi/button/lid}.

    \differences
    None.
    \enddifferences

    \sa QLidSensor, QLidFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QLidReading::QLidReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QLidReading::~QLidReading() = default;

/*!
    Returns the backLidClosed value. \c true if the lid of a laptop is closed.
*/
bool QLidReading::backLidClosed() const { return m_backLidClosed; }

/*!
    Sets the backLidClosed value to \a backLidClosed.
*/
void QLidReading::setBackLidClosed(bool backLidClosed) {
    if (m_backLidClosed == backLidClosed)
        return;
    m_backLidClosed = backLidClosed;
    emit backLidChanged(backLidClosed);
}

/*!
    Returns the frontLidClosed value. \c true if the front cover of a tablet is closed.
*/
bool QLidReading::frontLidClosed() const { return m_frontLidClosed; }

/*!
    Sets the frontLidClosed value to \a frontLidClosed.
*/
void QLidReading::setFrontLidClosed(bool frontLidClosed) {
    if (m_frontLidClosed == frontLidClosed)
        return;
    m_frontLidClosed = frontLidClosed;
    emit frontLidChanged(frontLidClosed);
}

/*!
    \class QLidFilter
    \inmodule NtSensors
    \brief The QLidFilter class is a filter for the readings of QLidSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QLidFilter::filter(QLidReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QLidFilter::filter(QSensorReading *reading) { return filter(static_cast<QLidReading *>(reading)); }

/*!
    \class QLidSensor
    \inmodule NtSensors
    \brief The QLidSensor class reads a lid sensor.
    \since 6.0

    A lid sensor tells whether the lid of a laptop, or the cover of a tablet, is closed. On Linux the lid is that of the ACPI button under \c{/proc/acpi/button/lid}. Each value is reported in reading() with readingChanged():

    \code
    QLidSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->backLidClosed();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QLidReading, {Using Nt Sensors}
*/

/*!
    \variable QLidSensor::sensorType

    The type of the sensor, \c QLidSensor, as QSensor::type() returns it.
*/
const char *const QLidSensor::sensorType = "QLidSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QLidSensor::QLidSensor(QObject *parent) : QSensor(sensorType, new QLidReading, parent) {}

/*!
    Destroys the sensor.
*/
QLidSensor::~QLidSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QLidReading *QLidSensor::reading() const { return static_cast<QLidReading *>(QSensor::reading()); }

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
    Constructs a reading with the given \a parent.
*/
QLightReading::QLightReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QLightReading::~QLightReading() = default;

/*!
    Returns the lux value. The illuminance, in lux.
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
    Constructs a reading with the given \a parent.
*/
QMagnetometerReading::QMagnetometerReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QMagnetometerReading::~QMagnetometerReading() = default;

/*!
    Returns the x value. The magnetic flux density along the x axis, in tesla.
*/
qreal QMagnetometerReading::x() const { return m_x; }

/*!
    Sets the x value to \a x.
*/
void QMagnetometerReading::setX(qreal x) { m_x = x; }

/*!
    Returns the y value. The magnetic flux density along the y axis, in tesla.
*/
qreal QMagnetometerReading::y() const { return m_y; }

/*!
    Sets the y value to \a y.
*/
void QMagnetometerReading::setY(qreal y) { m_y = y; }

/*!
    Returns the z value. The magnetic flux density along the z axis, in tesla.
*/
qreal QMagnetometerReading::z() const { return m_z; }

/*!
    Sets the z value to \a z.
*/
void QMagnetometerReading::setZ(qreal z) { m_z = z; }

/*!
    Returns the calibrationLevel value. How well the sensor is calibrated, from 0 to 1. Nt cannot tell, and reports 1.
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
bool QMagnetometerFilter::filter(QSensorReading *reading) {
    return filter(static_cast<QMagnetometerReading *>(reading));
}

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
    \class QOrientationReading
    \inmodule NtSensors
    \brief The QOrientationReading class holds a reading of an orientation sensor.
    \since 6.0

    An orientation sensor reports which side of the device faces up. Nt computes it from an accelerometer: the axis that carries most of gravity, when it carries more than 70 percent of it; otherwise the last orientation is kept.

    \differences
    None.
    \enddifferences

    \sa QOrientationSensor, QOrientationFilter
*/

/*!
    \enum QOrientationReading::Orientation

    This enum describes the values of the reading.

    \value Undefined The orientation is not known yet.
    \value TopUp The top edge points up.
    \value TopDown The top edge points down.
    \value LeftUp The left edge points up.
    \value RightUp The right edge points up.
    \value FaceUp The face points up, as when the device lies on a table.
    \value FaceDown The face points down.
*/

/*!
    Constructs a reading with the given \a parent.
*/
QOrientationReading::QOrientationReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QOrientationReading::~QOrientationReading() = default;

/*!
    Returns the orientation value. Which side of the device faces up.
*/
QOrientationReading::Orientation QOrientationReading::orientation() const { return m_orientation; }

/*!
    Sets the orientation value to \a orientation.
*/
void QOrientationReading::setOrientation(Orientation orientation) { m_orientation = orientation; }

/*!
    \class QOrientationFilter
    \inmodule NtSensors
    \brief The QOrientationFilter class is a filter for the readings of QOrientationSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QOrientationFilter::filter(QOrientationReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QOrientationFilter::filter(QSensorReading *reading) { return filter(static_cast<QOrientationReading *>(reading)); }

/*!
    \class QOrientationSensor
    \inmodule NtSensors
    \brief The QOrientationSensor class reads an orientation sensor.
    \since 6.0

    An orientation sensor reports which side of the device faces up. Nt computes it from an accelerometer: the axis that carries most of gravity, when it carries more than 70 percent of it; otherwise the last orientation is kept. Each value is reported in reading() with readingChanged():

    \code
    QOrientationSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->orientation();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QOrientationReading, {Using Nt Sensors}
*/

/*!
    \variable QOrientationSensor::sensorType

    The type of the sensor, \c QOrientationSensor, as QSensor::type() returns it.
*/
const char *const QOrientationSensor::sensorType = "QOrientationSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QOrientationSensor::QOrientationSensor(QObject *parent) : QSensor(sensorType, new QOrientationReading, parent) {}

/*!
    Destroys the sensor.
*/
QOrientationSensor::~QOrientationSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QOrientationReading *QOrientationSensor::reading() const {
    return static_cast<QOrientationReading *>(QSensor::reading());
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
    Constructs a reading with the given \a parent.
*/
QPressureReading::QPressureReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QPressureReading::~QPressureReading() = default;

/*!
    Returns the pressure value. The atmospheric pressure, in pascals. At sea level it is about 101325 Pa.
*/
qreal QPressureReading::pressure() const { return m_pressure; }

/*!
    Sets the pressure value to \a pressure.
*/
void QPressureReading::setPressure(qreal pressure) { m_pressure = pressure; }

/*!
    Returns the temperature value. The temperature of the sensor, in degrees Celsius, where the device measures it; otherwise 0.
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
    \class QProximityReading
    \inmodule NtSensors
    \brief The QProximityReading class holds a reading of a proximity sensor.
    \since 6.0

    A proximity sensor tells whether something is close to it, such as an ear to the screen of a phone. On Zephyr it is the proximity channel of a sensor driver, and on Android the proximity sensor.

    \differences
    None.
    \enddifferences

    \sa QProximitySensor, QProximityFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QProximityReading::QProximityReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QProximityReading::~QProximityReading() = default;

/*!
    Returns the close value. \c true if something is close to the sensor.
*/
bool QProximityReading::close() const { return m_close; }

/*!
    Sets the close value to \a close.
*/
void QProximityReading::setClose(bool close) { m_close = close; }

/*!
    \class QProximityFilter
    \inmodule NtSensors
    \brief The QProximityFilter class is a filter for the readings of QProximitySensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QProximityFilter::filter(QProximityReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QProximityFilter::filter(QSensorReading *reading) { return filter(static_cast<QProximityReading *>(reading)); }

/*!
    \class QProximitySensor
    \inmodule NtSensors
    \brief The QProximitySensor class reads a proximity sensor.
    \since 6.0

    A proximity sensor tells whether something is close to it, such as an ear to the screen of a phone. On Zephyr it is the proximity channel of a sensor driver, and on Android the proximity sensor. Each value is reported in reading() with readingChanged():

    \code
    QProximitySensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->close();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor.
    \enddifferences

    \sa QProximityReading, {Using Nt Sensors}
*/

/*!
    \variable QProximitySensor::sensorType

    The type of the sensor, \c QProximitySensor, as QSensor::type() returns it.
*/
const char *const QProximitySensor::sensorType = "QProximitySensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QProximitySensor::QProximitySensor(QObject *parent) : QSensor(sensorType, new QProximityReading, parent) {}

/*!
    Destroys the sensor.
*/
QProximitySensor::~QProximitySensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QProximityReading *QProximitySensor::reading() const { return static_cast<QProximityReading *>(QSensor::reading()); }

/*!
    \class QRotationReading
    \inmodule NtSensors
    \brief The QRotationReading class holds a reading of a rotation sensor.
    \since 6.0

    A rotation sensor reports the rotation of the device as three angles, in degrees, from a device lying face up with its top pointing to magnetic north. Nt computes x and y from an accelerometer and, where there is a magnetometer, z from the compass heading.

    \differences
    None.
    \enddifferences

    \sa QRotationSensor, QRotationFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QRotationReading::QRotationReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QRotationReading::~QRotationReading() = default;

/*!
    Returns the x value. The rotation around the x axis, in degrees from -90 to 90: positive when the top is raised.
*/
qreal QRotationReading::x() const { return m_x; }

/*!
    Returns the y value. The rotation around the y axis, in degrees from -180 to 180: positive when the right edge is raised.
*/
qreal QRotationReading::y() const { return m_y; }

/*!
    Returns the z value. The rotation around the z axis, in degrees from -180 to 180: positive when the top turns west of north. 0 when the sensor has no z.
*/
qreal QRotationReading::z() const { return m_z; }

/*!
    Sets the rotation to \a x, \a y and \a z degrees around the x, y and z axes.
*/
void QRotationReading::setFromEuler(qreal x, qreal y, qreal z) {
    m_x = x;
    m_y = y;
    m_z = z;
}

/*!
    \class QRotationFilter
    \inmodule NtSensors
    \brief The QRotationFilter class is a filter for the readings of QRotationSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QRotationFilter::filter(QRotationReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QRotationFilter::filter(QSensorReading *reading) { return filter(static_cast<QRotationReading *>(reading)); }

/*!
    \class QRotationSensor
    \inmodule NtSensors
    \brief The QRotationSensor class reads a rotation sensor.
    \since 6.0

    A rotation sensor reports the rotation of the device as three angles, in degrees, from a device lying face up with its top pointing to magnetic north. Nt computes x and y from an accelerometer and, where there is a magnetometer, z from the compass heading. Each value is reported in reading() with readingChanged():

    \code
    QRotationSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->x();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. The rotation is computed from an accelerometer and, for z, a magnetometer; it is not fused with a gyroscope.
    \enddifferences

    \sa QRotationReading, {Using Nt Sensors}
*/

/*!
    \variable QRotationSensor::sensorType

    The type of the sensor, \c QRotationSensor, as QSensor::type() returns it.
*/
const char *const QRotationSensor::sensorType = "QRotationSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QRotationSensor::QRotationSensor(QObject *parent) : QSensor(sensorType, new QRotationReading, parent) {}

/*!
    Destroys the sensor.
*/
QRotationSensor::~QRotationSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QRotationReading *QRotationSensor::reading() const { return static_cast<QRotationReading *>(QSensor::reading()); }

/*!
    Returns \c true if the sensor reports the rotation around the z axis. Nt sets it when the sensor
    starts: it is \c true when a magnetometer gives the heading.
*/
bool QRotationSensor::hasZ() const { return m_hasZ; }

/*!
    Sets whether the sensor reports the rotation around the z axis to \a hasZ; for a backend.
*/
void QRotationSensor::setHasZ(bool hasZ) {
    if (m_hasZ == hasZ)
        return;
    m_hasZ = hasZ;
    emit hasZChanged(hasZ);
}

/*!
    \class QTapReading
    \inmodule NtSensors
    \brief The QTapReading class holds a reading of a tap sensor.
    \since 6.0

    A tap sensor reports taps on the device. No backend of Nt has one; a QSensorBackend can provide it.

    \differences
    None.
    \enddifferences

    \sa QTapSensor, QTapFilter
*/

/*!
    \enum QTapReading::TapDirection

    This enum describes the values of the reading.

    \value Undefined The direction is not known.
    \value X Along the x axis.
    \value Y Along the y axis.
    \value Z Along the z axis.
    \value X_Pos Towards the positive x direction.
    \value Y_Pos Towards the positive y direction.
    \value Z_Pos Towards the positive z direction.
    \value X_Neg Towards the negative x direction.
    \value Y_Neg Towards the negative y direction.
    \value Z_Neg Towards the negative z direction.
    \value X_Both Either way along the x axis.
    \value Y_Both Either way along the y axis.
    \value Z_Both Either way along the z axis.
*/

/*!
    Constructs a reading with the given \a parent.
*/
QTapReading::QTapReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QTapReading::~QTapReading() = default;

/*!
    Returns the tapDirection value. The direction of the tap.
*/
QTapReading::TapDirection QTapReading::tapDirection() const { return m_tapDirection; }

/*!
    Sets the tapDirection value to \a tapDirection.
*/
void QTapReading::setTapDirection(TapDirection tapDirection) { m_tapDirection = tapDirection; }

/*!
    Returns the doubleTap value. \c true for a double tap.
*/
bool QTapReading::isDoubleTap() const { return m_doubleTap; }

/*!
    Sets the doubleTap value to \a doubleTap.
*/
void QTapReading::setDoubleTap(bool doubleTap) { m_doubleTap = doubleTap; }

/*!
    \class QTapFilter
    \inmodule NtSensors
    \brief The QTapFilter class is a filter for the readings of QTapSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QTapFilter::filter(QTapReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QTapFilter::filter(QSensorReading *reading) { return filter(static_cast<QTapReading *>(reading)); }

/*!
    \class QTapSensor
    \inmodule NtSensors
    \brief The QTapSensor class reads a tap sensor.
    \since 6.0

    A tap sensor reports taps on the device. No backend of Nt has one; a QSensorBackend can provide it. Each value is reported in reading() with readingChanged():

    \code
    QTapSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->tapDirection();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. Only a QSensorBackend can provide taps.
    \enddifferences

    \sa QTapReading, {Using Nt Sensors}
*/

/*!
    \variable QTapSensor::sensorType

    The type of the sensor, \c QTapSensor, as QSensor::type() returns it.
*/
const char *const QTapSensor::sensorType = "QTapSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QTapSensor::QTapSensor(QObject *parent) : QSensor(sensorType, new QTapReading, parent) {}

/*!
    Destroys the sensor.
*/
QTapSensor::~QTapSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QTapReading *QTapSensor::reading() const { return static_cast<QTapReading *>(QSensor::reading()); }

/*!
    Returns \c true if double taps are reported as such, or \c false if each tap is reported
    alone. It is \c true until set.
*/
bool QTapSensor::returnDoubleTapEvents() const { return m_returnDoubleTapEvents; }

/*!
    Sets whether double taps are reported as such to \a returnDoubleTapEvents, for the backend to follow.
*/
void QTapSensor::setReturnDoubleTapEvents(bool returnDoubleTapEvents) {
    if (m_returnDoubleTapEvents == returnDoubleTapEvents)
        return;
    m_returnDoubleTapEvents = returnDoubleTapEvents;
    emit returnDoubleTapEventsChanged(returnDoubleTapEvents);
}

/*!
    \class QTiltReading
    \inmodule NtSensors
    \brief The QTiltReading class holds a reading of a tilt sensor.
    \since 6.0

    A tilt sensor reports how far the device is tilted around its x and y axes, in degrees. Nt computes it from an accelerometer: x is atan(y / sqrt(x² + z²)) and y is atan(x / sqrt(y² + z²)) of the acceleration, less what calibrate() took as level.

    \differences
    None.
    \enddifferences

    \sa QTiltSensor, QTiltFilter
*/

/*!
    Constructs a reading with the given \a parent.
*/
QTiltReading::QTiltReading(QObject *parent) : QSensorReading(parent) {}

/*!
    Destroys the reading.
*/
QTiltReading::~QTiltReading() = default;

/*!
    Returns the yRotation value. The tilt around the y axis, in degrees from -90 to 90.
*/
qreal QTiltReading::yRotation() const { return m_yRotation; }

/*!
    Sets the yRotation value to \a yRotation.
*/
void QTiltReading::setYRotation(qreal yRotation) { m_yRotation = yRotation; }

/*!
    Returns the xRotation value. The tilt around the x axis, in degrees from -90 to 90.
*/
qreal QTiltReading::xRotation() const { return m_xRotation; }

/*!
    Sets the xRotation value to \a xRotation.
*/
void QTiltReading::setXRotation(qreal xRotation) { m_xRotation = xRotation; }

/*!
    \class QTiltFilter
    \inmodule NtSensors
    \brief The QTiltFilter class is a filter for the readings of QTiltSensor.
    \since 6.0

    Reimplement filter() and add the filter with QSensor::addFilter().

    \differences
    The override of QSensorFilter::filter() is protected rather than private, so that a subclass can
    bring it in with \c{using} and keep \c{-Woverloaded-virtual} quiet.
    \enddifferences

    \sa QSensorFilter
*/

/*!
    \fn bool QTiltFilter::filter(QTiltReading *reading)

    Looks at \a reading, which it may change, and returns \c true to let the sensor report it or
    \c false to drop it.
*/

/*!
    \internal
*/
bool QTiltFilter::filter(QSensorReading *reading) { return filter(static_cast<QTiltReading *>(reading)); }

/*!
    \class QTiltSensor
    \inmodule NtSensors
    \brief The QTiltSensor class reads a tilt sensor.
    \since 6.0

    A tilt sensor reports how far the device is tilted around its x and y axes, in degrees. Nt computes it from an accelerometer: x is atan(y / sqrt(x² + z²)) and y is atan(x / sqrt(y² + z²)) of the acceleration, less what calibrate() took as level. Each value is reported in reading() with readingChanged():

    \code
    QTiltSensor sensor;
    QObject::connect(&sensor, &QSensor::readingChanged, [&sensor] {
        qDebug() << sensor.reading()->yRotation();
    });
    sensor.start();
    \endcode

    \differences
    As for QSensor. calibrate() takes the last reading as level and does not wait for the device to rest.
    \enddifferences

    \sa QTiltReading, {Using Nt Sensors}
*/

/*!
    \variable QTiltSensor::sensorType

    The type of the sensor, \c QTiltSensor, as QSensor::type() returns it.
*/
const char *const QTiltSensor::sensorType = "QTiltSensor";

/*!
    Constructs a sensor with the given \a parent.
*/
QTiltSensor::QTiltSensor(QObject *parent) : QSensor(sensorType, new QTiltReading, parent) {
    // calibrate() takes the angles of the last sample as level; they are taken off before filters run.
    QtSensorsPrivate::SensorAccess::setAdjuster(this, [this](QList<qreal> &values) {
        if (values.size() >= 2) {
            values[0] -= m_yOffset;
            values[1] -= m_xOffset;
        }
    });
}

/*!
    Destroys the sensor.
*/
QTiltSensor::~QTiltSensor() = default;

/*!
    Returns the reading that the sensor updates.
*/
QTiltReading *QTiltSensor::reading() const { return static_cast<QTiltReading *>(QSensor::reading()); }

/*!
    Takes the tilt of the last reading as level, so that later readings are relative to it.
*/
void QTiltSensor::calibrate() {
    const QList<qreal> raw = QtSensorsPrivate::SensorAccess::lastValues(this);
    if (raw.size() >= 2) {
        m_yOffset = raw[0];
        m_xOffset = raw[1];
    }
}

#include "moc_qaccelerometer.cpp"            // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qaltimeter.cpp"                // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qambientlightsensor.cpp"       // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qambienttemperaturesensor.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qcompass.cpp"                  // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qgyroscope.cpp"                // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qholstersensor.cpp"            // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qhumiditysensor.cpp"           // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qirproximitysensor.cpp"        // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qlidsensor.cpp"                // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qlightsensor.cpp"              // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qmagnetometer.cpp"             // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qorientationsensor.cpp"        // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qpressuresensor.cpp"           // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qproximitysensor.cpp"          // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qrotationsensor.cpp"           // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qtapsensor.cpp"                // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qtiltsensor.cpp"               // NOLINT(bugprone-suspicious-include): generated by ntmoc
