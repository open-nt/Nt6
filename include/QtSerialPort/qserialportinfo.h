// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

class QSerialPort;

namespace QtPrivate {
class SerialPortScanner;
}

class QSerialPortInfo {
public:
    QSerialPortInfo();
    explicit QSerialPortInfo(const QSerialPort &port);
    explicit QSerialPortInfo(const QString &name);

    void swap(QSerialPortInfo &other) noexcept;

    QString portName() const;
    QString systemLocation() const;
    QString description() const;
    QString manufacturer() const;
    QString serialNumber() const;
    quint16 vendorIdentifier() const;
    quint16 productIdentifier() const;
    bool hasVendorIdentifier() const;
    bool hasProductIdentifier() const;
    bool isNull() const;

    static QList<qint32> standardBaudRates();
    static QList<QSerialPortInfo> availablePorts();

private:
    friend class QtPrivate::SerialPortScanner;

    QString m_portName;
    QString m_systemLocation;
    QString m_description;
    QString m_manufacturer;
    QString m_serialNumber;
    quint16 m_vendor = 0;
    quint16 m_product = 0;
    bool m_hasVendor = false;
    bool m_hasProduct = false;
};
