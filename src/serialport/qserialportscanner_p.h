// MIT License - see LICENSE
#pragma once

#include <QtSerialPort/qserialportinfo.h>

namespace QtPrivate {

class SerialPortScanner {
public:
    // Lists the serial ports in a sysfs tree. The roots are parameters so that
    // tests can describe hardware the machine running them does not have.
    static QList<QSerialPortInfo> scan(const QString &sysRoot, const QString &devRoot);
};

} // namespace QtPrivate
