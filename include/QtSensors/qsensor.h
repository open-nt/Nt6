// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpair.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <memory>

class QSensor;
class QSensorPrivate;
namespace QtSensorsPrivate {
struct SensorAccess;
}

using qrange = QPair<int, int>;
using qrangelist = QList<qrange>;

struct qoutputrange {
    qreal minimum;
    qreal maximum;
    qreal accuracy;
};
using qoutputrangelist = QList<qoutputrange>;

class QSensorReading : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 timestamp READ timestamp)
public:
    ~QSensorReading() override;

    quint64 timestamp() const;
    void setTimestamp(quint64 timestamp);

    int valueCount() const;
    QVariant value(int index) const;

protected:
    explicit QSensorReading(QObject *parent = nullptr);

private:
    quint64 m_timestamp = 0;
};

class QSensorFilter {
    friend class QSensor;

public:
    virtual bool filter(QSensorReading *reading) = 0;

protected:
    QSensorFilter();
    virtual ~QSensorFilter();
    QSensorFilter(const QSensorFilter &) = delete;
    QSensorFilter &operator=(const QSensorFilter &) = delete;
    virtual void setSensor(QSensor *sensor);

    QSensor *m_sensor = nullptr;
};

class QSensor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QByteArray identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QByteArray type READ type CONSTANT)
    Q_PROPERTY(bool connectedToBackend READ isConnectedToBackend)
    Q_PROPERTY(int dataRate READ dataRate WRITE setDataRate NOTIFY dataRateChanged)
    Q_PROPERTY(QSensorReading *reading READ reading NOTIFY readingChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString description READ description)
    Q_PROPERTY(int error READ error NOTIFY sensorError)
    Q_PROPERTY(bool alwaysOn READ isAlwaysOn WRITE setAlwaysOn NOTIFY alwaysOnChanged)
    Q_PROPERTY(bool skipDuplicates READ skipDuplicates WRITE setSkipDuplicates NOTIFY skipDuplicatesChanged)
public:
    enum Feature {
        Buffering,
        AlwaysOn,
        GeoValues,
        FieldOfView,
        AccelerationMode,
        SkipDuplicates,
        AxesOrientation,
        PressureSensorTemperature,
        Reserved = 257
    };
    Q_ENUM(Feature)

    explicit QSensor(const QByteArray &type, QObject *parent = nullptr);
    ~QSensor() override;

    QByteArray identifier() const;
    void setIdentifier(const QByteArray &identifier);
    QByteArray type() const;

    Q_INVOKABLE bool connectToBackend();
    bool isConnectedToBackend() const;
    bool isBusy() const;

    void setActive(bool active);
    bool isActive() const;
    bool isAlwaysOn() const;
    void setAlwaysOn(bool alwaysOn);
    bool skipDuplicates() const;
    void setSkipDuplicates(bool skipDuplicates);

    qrangelist availableDataRates() const;
    int dataRate() const;
    void setDataRate(int rate);
    qoutputrangelist outputRanges() const;
    int outputRange() const;
    void setOutputRange(int index);
    QString description() const;
    int error() const;

    void addFilter(QSensorFilter *filter);
    void removeFilter(QSensorFilter *filter);
    QList<QSensorFilter *> filters() const;

    QSensorReading *reading() const;

    static QList<QByteArray> sensorTypes();
    static QList<QByteArray> sensorsForType(const QByteArray &type);
    static QByteArray defaultSensorForType(const QByteArray &type);

    Q_INVOKABLE bool isFeatureSupported(QSensor::Feature feature) const;

public slots:
    bool start();
    void stop();

signals:
    /*!
        This signal is emitted when the sensor becomes busy or free.

        \sa isBusy()
    */
    void busyChanged() { emitSignal(&QSensor::busyChanged); }
    /*!
        This signal is emitted when the sensor starts or stops.

        \sa isActive()
    */
    void activeChanged() { emitSignal(&QSensor::activeChanged); }
    /*!
        This signal is emitted when reading() holds a new value.
    */
    void readingChanged() { emitSignal(&QSensor::readingChanged); }
    /*!
        This signal is emitted when the sensor fails with \a error, a negative \c errno value.

        \sa error()
    */
    void sensorError(int error) { emitSignal(&QSensor::sensorError, error); }
    /*!
        This signal is emitted when sensors are added or removed. Nt does not watch for that, so it
        is not emitted.
    */
    void availableSensorsChanged() { emitSignal(&QSensor::availableSensorsChanged); }
    /*!
        This signal is emitted when isAlwaysOn() changes.
    */
    void alwaysOnChanged() { emitSignal(&QSensor::alwaysOnChanged); }
    /*!
        This signal is emitted when dataRate() changes.
    */
    void dataRateChanged() { emitSignal(&QSensor::dataRateChanged); }
    /*!
        This signal is emitted when skipDuplicates() changes to \a skipDuplicates.
    */
    void skipDuplicatesChanged(bool skipDuplicates) { emitSignal(&QSensor::skipDuplicatesChanged, skipDuplicates); }
    /*!
        This signal is emitted when identifier() changes.
    */
    void identifierChanged() { emitSignal(&QSensor::identifierChanged); }

protected:
    QSensor(const QByteArray &type, QSensorReading *reading, QObject *parent);

private:
    friend struct QtSensorsPrivate::SensorAccess;
    std::unique_ptr<QSensorPrivate> d;
};
