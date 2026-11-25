// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtSerialBus/qcanbusdeviceinfo.h>

class QCanBusDevice;
class QCanBusFactory;

class QCanBus : public QObject {
    Q_OBJECT
public:
    static QCanBus *instance();

    QStringList plugins() const;
    QList<QCanBusDeviceInfo> availableDevices(const QString &plugin, QString *errorMessage = nullptr) const;
    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage = nullptr) const;
    QCanBusDevice *createDevice(const QString &plugin, const QString &interfaceName,
                                QString *errorMessage = nullptr) const;

    void registerFactory(const QString &plugin, QCanBusFactory *factory);

private:
    explicit QCanBus(QObject *parent = nullptr);
};
