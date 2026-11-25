// MIT License - see LICENSE
//
// Overview page about Nt Sensors for the NDoc documentation. This file holds only a documentation comment and is not
// part of the library.

/*!
    \page nt-sensors.html
    \title Using Nt Sensors
    \brief Reading accelerometers, gyroscopes, magnetometers and environmental sensors on Linux and Zephyr.

    Nt Sensors is a subset of Qt Sensors. It depends only on Nt Core, so it also runs headless and on
    Zephyr. Link with \c Nt::Sensors (or \c Qt6::Sensors):

    \code
    find_package(Nt6 REQUIRED COMPONENTS Sensors)
    target_link_libraries(app PRIVATE Nt::Sensors)
    \endcode

    \section1 Reading a Sensor

    Make the sensor of the kind you want, connect to readingChanged(), and start it. The value is in
    reading(), in the units of Qt Sensors:

    \code
    QAccelerometer accelerometer;
    accelerometer.setDataRate(50);   // readings a second; 10 if not set
    QObject::connect(&accelerometer, &QSensor::readingChanged, [&accelerometer] {
        const QAccelerometerReading *r = accelerometer.reading();
        qDebug() << r->x() << r->y() << r->z() << "m/s^2";
    });
    if (!accelerometer.start())
        qWarning() << "no accelerometer, error" << accelerometer.error();
    \endcode

    \table
    \header \li Class \li Reading \li Values
    \row \li QAccelerometer \li QAccelerometerReading \li x, y, z in m/s\unicode{0xb2}
    \row \li QGyroscope \li QGyroscopeReading \li x, y, z in degrees a second
    \row \li QMagnetometer \li QMagnetometerReading \li x, y, z in tesla
    \row \li QPressureSensor \li QPressureReading \li pressure in pascals, temperature in \unicode{0xb0}C
    \row \li QAmbientTemperatureSensor \li QAmbientTemperatureReading \li temperature in \unicode{0xb0}C
    \row \li QHumiditySensor \li QHumidityReading \li relative humidity in percent
    \row \li QLightSensor \li QLightReading \li illuminance in lux
    \endtable

    QSensor::sensorTypes() lists the kinds the machine has and QSensor::sensorsForType() the sensors of
    a kind, the default first. Choose another than the default with QSensor::setIdentifier() before
    starting. A QSensorFilter, such as a subclass of QAccelerometerFilter, can smooth, change or drop
    readings before they are reported.

    \section1 Linux

    The sensors are the Industrial I/O (IIO) devices of the kernel, under \c{/sys/bus/iio/devices}:
    the identifier is the name of the device directory, such as \c{iio:device0}, and the description
    is its \c name. A device with several kinds of channel, such as a BME280 with pressure,
    temperature and humidity, is listed once for each. The channels are read through sysfs at the data
    rate, as their \c _input files or as \c{(_raw + _offset) * _scale}, and converted from the units of
    IIO. The files have to be readable by the user, which a udev rule can arrange.

    \section1 Zephyr

    Set \c CONFIG_SENSOR and \c CONFIG_NT_SENSORS, and the driver of the sensor, and link with
    \c Nt::Sensors. The sensors listed are those that the usual aliases of the devicetree name:
    \c accel0, \c gyro0, \c magn0, \c press0, \c ambient-temp0, \c die-temp0, \c humidity0,
    \c light0, and \c accel1, \c gyro1 and \c magn1. Any other sensor device can be read by setting
    its name, as \c device_get_binding() takes it, as the identifier. A sample is fetched with
    \c sensor_sample_fetch() in the thread of the sensor, which blocks while the bus transfers it.

    \code
    / {
        aliases {
            accel0 = &lsm6dso;
        };
    };
    \endcode

    \section1 What Is Missing

    Readings are polled from the event loop; there is no buffering or batching, so rates above a few
    hundred a second are not practical. The orientation, rotation, compass, tilt, proximity, tap and
    other derived sensors of Qt Sensors are not there, nor sensor gestures and the QML types. Android
    has no backend yet.

    \sa {Nt Sensors C++ Classes}, {Nt on Zephyr}
*/
