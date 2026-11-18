// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qpointer.h>
#include <QtPositioning/qgeopositioninfosource.h>

class QIODevice;
class QTimer;

class QNmeaPositionInfoSource : public QGeoPositionInfoSource {
    Q_OBJECT

public:
    enum UpdateMode { RealTimeMode = 1, SimulationMode };
    Q_ENUM(UpdateMode)

    explicit QNmeaPositionInfoSource(UpdateMode updateMode, QObject *parent = nullptr);
    ~QNmeaPositionInfoSource() override;

    void setUserEquivalentRangeError(double uere) { m_uere = uere; }
    double userEquivalentRangeError() const { return m_uere; }

    UpdateMode updateMode() const { return m_mode; }
    void setDevice(QIODevice *device);
    QIODevice *device() const;

    void setUpdateInterval(int msec) override;
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const override;
    PositioningMethods supportedPositioningMethods() const override;
    int minimumUpdateInterval() const override;
    Error error() const override { return m_error; }

public slots:
    void startUpdates() override;
    void stopUpdates() override;
    void requestUpdate(int timeout = 0) override;

protected:
    virtual bool parsePosInfoFromNmeaData(const char *data, int size, QGeoPositionInfo *posInfo, bool *hasFix);

private:
    // The sentences of one epoch (one time of day), merged into one update.
    struct Epoch {
        QGeoPositionInfo info;
        bool started = false;
        bool hasPosition = false;
        bool fix = true;
    };

    bool openDevice();
    void readAvailable();
    void processLine(const QByteArray &line);
    void flushEpoch();
    void deliver(const QGeoPositionInfo &info);
    void emitThrottled();
    void simulateNext();
    void setError(Error error);

    UpdateMode m_mode;
    QPointer<QIODevice> m_device;
    double m_uere = qQNaN();
    Error m_error = NoError;
    bool m_running = false;
    bool m_requested = false;
    bool m_collecting = false;
    QByteArray m_buffer;
    QDate m_date;
    Epoch m_epoch;
    QGeoPositionInfo m_lastKnown;
    QGeoPositionInfo m_latest;
    QDateTime m_lastEmitted;
    QTimer *m_requestTimer = nullptr;
    QTimer *m_intervalTimer = nullptr;
    QTimer *m_simulationTimer = nullptr;
    QList<QGeoPositionInfo> m_simulation;
    qsizetype m_simulationIndex = -1;
};
