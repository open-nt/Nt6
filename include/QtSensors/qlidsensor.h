// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QLidReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(bool backLidClosed READ backLidClosed)
    Q_PROPERTY(bool frontLidClosed READ frontLidClosed)
public:
    explicit QLidReading(QObject *parent = nullptr);
    ~QLidReading() override;

    bool backLidClosed() const;
    void setBackLidClosed(bool backLidClosed);
    bool frontLidClosed() const;
    void setFrontLidClosed(bool frontLidClosed);

signals:
    /*!
        This signal is emitted when backLidClosed() changes to \a closed.
    */
    void backLidChanged(bool closed) { emitSignal(&QLidReading::backLidChanged, closed); }
    /*!
        This signal is emitted when frontLidClosed() changes to \a closed.
    */
    void frontLidChanged(bool closed) { emitSignal(&QLidReading::frontLidChanged, closed); }

private:
    bool m_backLidClosed = false;
    bool m_frontLidClosed = false;
};

class QLidFilter : public QSensorFilter {
public:
    virtual bool filter(QLidReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QLidSensor : public QSensor {
    Q_OBJECT
public:
    explicit QLidSensor(QObject *parent = nullptr);
    ~QLidSensor() override;

    QLidReading *reading() const;

    static const char *const sensorType;
};
