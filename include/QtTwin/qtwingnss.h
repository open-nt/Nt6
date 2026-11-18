// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qrandom.h>
#include <QtPositioning/qgeopositioninfosource.h>
#include <QtPositioning/qgeoshape.h>
#include <QtTwin/qtwinclock.h>
#include <QtTwin/qtwingeodesy.h>

#include <array>
#include <functional>

class QIODevice;
class QTimer;

namespace QtTwin {

struct GnssErrorModel {
    double horizontalBias = 1.5;    // metres, standard deviation per axis of the slowly varying error
    double verticalBias = 3.0;      // metres
    double correlationTime = 300.0; // seconds, of the bias (first-order Gauss-Markov)
    double horizontalNoise = 0.5;   // metres, standard deviation per axis of the error of each fix
    double verticalNoise = 1.0;     // metres
    double hdop = 0.9;
    double vdop = 1.4;
    int satellites = 9;
    quint32 seed = 1;

    static GnssErrorModel perfect();
};

class NmeaWriter {
public:
    static QByteArray sentence(const QByteArray &body);
    static QByteArray gga(const QGeoPositionInfo &info, int satellites, double hdop, double geoidSeparation);
    static QByteArray rmc(const QGeoPositionInfo &info);
    static QByteArray vtg(const QGeoPositionInfo &info);
    static QByteArray gsa(double pdop, double hdop, double vdop);
};

class GnssReceiver : public QGeoPositionInfoSource {
    Q_OBJECT

public:
    using Trajectory = std::function<GeoPosition(const QDateTime &time)>;

    explicit GnssReceiver(QObject *parent = nullptr);
    ~GnssReceiver() override;

    void setTrajectory(const Trajectory &trajectory);
    void setTruePosition(const GeoPosition &position);
    void setClock(SimulationClock *clock);
    SimulationClock *clock() const;
    void setErrorModel(const GnssErrorModel &model);
    GnssErrorModel errorModel() const { return m_model; }
    void setDeniedAreas(const QList<QGeoShape> &areas);
    QList<QGeoShape> deniedAreas() const { return m_denied; }
    void setGeoidUndulation(double metres) { m_undulation = metres; }
    double geoidUndulation() const { return m_undulation; }
    void setNmeaOutput(QIODevice *device);
    QIODevice *nmeaOutput() const;

    QGeoPositionInfo sample(const QDateTime &time);

    void setUpdateInterval(int msec) override;
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const override;
    PositioningMethods supportedPositioningMethods() const override;
    int minimumUpdateInterval() const override;
    Error error() const override { return m_error; }

    static void registerSource();

public slots:
    void startUpdates() override;
    void stopUpdates() override;
    void requestUpdate(int timeout = 0) override;

private:
    void onClockTime(const QDateTime &time);
    void produce(const QDateTime &time);
    double gaussian();
    int interval() const;

    Trajectory m_trajectory;
    QPointer<SimulationClock> m_clock;
    QPointer<QIODevice> m_nmea;
    GnssErrorModel m_model;
    QList<QGeoShape> m_denied;
    double m_undulation = 0.0;
    Error m_error = NoError;
    bool m_running = false;
    bool m_requested = false;
    bool m_denying = false;
    QGeoPositionInfo m_lastKnown;
    QDateTime m_nextSample;
    QDateTime m_lastSample;
    std::array<double, 3> m_bias{};
    QRandomGenerator m_random;
    double m_spareGaussian = 0.0;
    bool m_hasSpare = false;
    QTimer *m_timer = nullptr;
    QTimer *m_requestTimer = nullptr;
};

} // namespace QtTwin
