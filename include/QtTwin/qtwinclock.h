// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qobject.h>

class QTimer;

namespace QtTwin {

class SimulationClock : public QObject {
    Q_OBJECT
    Q_PROPERTY(QDateTime time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit SimulationClock(QObject *parent = nullptr);
    ~SimulationClock() override;

    QDateTime time() const;
    void setTime(const QDateTime &time);
    double rate() const { return m_rate; }
    void setRate(double rate);
    bool isRunning() const { return m_running; }
    void setTickInterval(int msec);
    int tickInterval() const { return m_tickInterval; }

    static int leapSeconds(const QDateTime &utc);
    static QDateTime toGpsTime(const QDateTime &utc);

public slots:
    void start();
    void pause();
    void advance(qint64 msec);

signals:
    void timeChanged(const QDateTime &time);
    void rateChanged(double rate);
    void runningChanged(bool running);

private:
    void tick();

    QDateTime m_time;
    double m_rate = 1.0;
    bool m_running = false;
    int m_tickInterval = 10;
    QElapsedTimer m_wall;
    qint64 m_wallAtLastTick = 0;
    double m_carry = 0.0;
    QTimer *m_timer = nullptr;
};

} // namespace QtTwin
