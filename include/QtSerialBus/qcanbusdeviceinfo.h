// MIT License - see LICENSE
#pragma once

#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QCanBusDeviceInfoPrivate;

class QCanBusDeviceInfo {
public:
    QCanBusDeviceInfo() = delete;
    QCanBusDeviceInfo(const QCanBusDeviceInfo &other);
    QCanBusDeviceInfo(QCanBusDeviceInfo &&other) noexcept;
    QCanBusDeviceInfo &operator=(const QCanBusDeviceInfo &other);
    QCanBusDeviceInfo &operator=(QCanBusDeviceInfo &&other) noexcept;
    ~QCanBusDeviceInfo();

    QString plugin() const;
    QString name() const;
    QString description() const;
    QString serialNumber() const;
    QString alias() const;
    int channel() const;
    bool hasFlexibleDataRate() const;
    bool isVirtual() const;

private:
    friend class QCanBusDevice;
    explicit QCanBusDeviceInfo(QCanBusDeviceInfoPrivate *d);
    QSharedDataPointer<QCanBusDeviceInfoPrivate> d_ptr;
};
