// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtPositioning/qgeosatelliteinfosource.h>

class QIODevice;
class QTimer;

class QNmeaSatelliteInfoSource : public QGeoSatelliteInfoSource {
    Q_OBJECT

public:
    enum UpdateMode { RealTimeMode = 1, SimulationMode };
    Q_ENUM(UpdateMode)

    static inline const QString SimulationUpdateInterval = QStringLiteral("nmea.satellite_info_simulation_interval");

    explicit QNmeaSatelliteInfoSource(UpdateMode mode, QObject *parent = nullptr);
    ~QNmeaSatelliteInfoSource() override;

    UpdateMode updateMode() const { return m_mode; }
    void setDevice(QIODevice *source);
    QIODevice *device() const;

    void setUpdateInterval(int msec) override;
    int minimumUpdateInterval() const override;
    Error error() const override { return m_error; }

    bool setBackendProperty(const QString &name, const QVariant &value) override;
    QVariant backendProperty(const QString &name) const override;

public slots:
    void startUpdates() override;
    void stopUpdates() override;
    void requestUpdate(int timeout = 0) override;

protected:
    enum SatelliteInfoParseStatus { NotParsed = 0, PartiallyParsed, FullyParsed };

    virtual QGeoSatelliteInfo::SatelliteSystem parseSatellitesInUseFromNmea(const char *data, int size,
                                                                            QList<int> &pnrsInUse);
    virtual SatelliteInfoParseStatus parseSatelliteInfoFromNmea(const char *data, int size,
                                                                QList<QGeoSatelliteInfo> &infos,
                                                                QGeoSatelliteInfo::SatelliteSystem &system);

private:
    struct Snapshot {
        QList<QGeoSatelliteInfo> inView;
        QList<QGeoSatelliteInfo> inUse;
    };

    bool openDevice();
    void readAvailable();
    void processLine(const QByteArray &line);
    Snapshot snapshot() const;
    void flush();
    void deliver(const Snapshot &snapshot);
    void simulateNext();
    void setError(Error error);

    UpdateMode m_mode;
    QPointer<QIODevice> m_device;
    Error m_error = NoError;
    bool m_running = false;
    bool m_requested = false;
    bool m_collecting = false;
    bool m_changed = false;
    int m_simulationInterval = 100;
    QByteArray m_buffer;
    QByteArray m_lastTime;
    QMap<int, QList<QGeoSatelliteInfo>> m_inView;        // by system, the last complete GSV set
    QMap<int, QList<QGeoSatelliteInfo>> m_partialInView; // by system, the GSV set being received
    QMap<int, QList<int>> m_inUse;                       // by system, from GSA
    QList<Snapshot> m_simulation;
    qsizetype m_simulationIndex = -1;
    QTimer *m_requestTimer = nullptr;
    QTimer *m_simulationTimer = nullptr;
};
