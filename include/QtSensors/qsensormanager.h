// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

class QSensor;
class QSensorBackend;

class QSensorBackendFactory {
public:
    virtual QSensorBackend *createBackend(QSensor *sensor) = 0;

protected:
    QSensorBackendFactory();
    virtual ~QSensorBackendFactory();
    QSensorBackendFactory(const QSensorBackendFactory &) = delete;
    QSensorBackendFactory &operator=(const QSensorBackendFactory &) = delete;
};

class QSensorManager {
public:
    static void registerBackend(const QByteArray &type, const QByteArray &identifier, QSensorBackendFactory *factory);
    static void unregisterBackend(const QByteArray &type, const QByteArray &identifier);
    static bool isBackendRegistered(const QByteArray &type, const QByteArray &identifier);
    static QSensorBackend *createBackend(QSensor *sensor);
    static void setDefaultBackend(const QByteArray &type, const QByteArray &identifier);
};
