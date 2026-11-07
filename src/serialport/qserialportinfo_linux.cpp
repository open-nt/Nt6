// MIT License - see LICENSE
#include "qserialportscanner_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

#include <climits>
#include <cstdlib>

namespace {

QByteArray attribute(const QString &dir, const QString &name) {
    QFile file(dir + QChar(u'/') + name);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll().trimmed();
}

QString canonical(const QString &path) {
    char resolved[PATH_MAX];
    return ::realpath(path.toUtf8().constData(), resolved) ? QString::fromUtf8(resolved) : QString();
}

} // namespace

namespace QtPrivate {

QList<QSerialPortInfo> SerialPortScanner::scan(const QString &sysRoot, const QString &devRoot) {
    QList<QSerialPortInfo> ports;
    const QString ttyRoot = sysRoot + QString("/class/tty");
    const QString top = canonical(sysRoot);
    const QStringList names = QDir(ttyRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : names) {
        const QString tty = ttyRoot + QChar(u'/') + name;
        // Virtual terminals and pseudo terminals have no hardware behind them.
        QString device = canonical(tty + QString("/device"));
        if (device.isEmpty())
            continue;
        // The 8250 driver registers ports that do not exist, with type 0 (unknown).
        if (attribute(tty, QString("type")) == "0")
            continue;
        const QString location = devRoot + QChar(u'/') + name;
        if (!QFileInfo::exists(location))
            continue;

        QSerialPortInfo info;
        info.m_portName = name;
        info.m_systemLocation = location;
        // The identity of the hardware is on one of the device's ancestors.
        for (; device.size() > top.size(); device = device.left(device.lastIndexOf(QChar(u'/')))) {
            const QByteArray usbVendor = attribute(device, QString("idVendor"));
            if (!usbVendor.isEmpty()) {
                info.m_vendor = usbVendor.toUShort(&info.m_hasVendor, 16);
                info.m_product = attribute(device, QString("idProduct")).toUShort(&info.m_hasProduct, 16);
                info.m_description = QString::fromUtf8(attribute(device, QString("product")));
                info.m_manufacturer = QString::fromUtf8(attribute(device, QString("manufacturer")));
                info.m_serialNumber = QString::fromUtf8(attribute(device, QString("serial")));
                break;
            }
            const QByteArray pciVendor = attribute(device, QString("vendor"));
            if (pciVendor.startsWith("0x")) {
                info.m_vendor = pciVendor.mid(2).toUShort(&info.m_hasVendor, 16);
                info.m_product = attribute(device, QString("device")).mid(2).toUShort(&info.m_hasProduct, 16);
                break;
            }
        }
        ports.append(info);
    }
    return ports;
}

} // namespace QtPrivate
