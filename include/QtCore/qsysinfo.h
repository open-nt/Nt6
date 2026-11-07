// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>

class QSysInfo {
public:
    /*!
        \enum QSysInfo::Sizes

        \value WordSize The size in bits of a pointer on the platform the program was built for.
    */
    enum Sizes { WordSize = static_cast<int>(sizeof(void *) << 3) };

    /*!
        \enum QSysInfo::Endian

        \value BigEndian Multi-byte values are stored with the most significant byte first.
        \value LittleEndian Multi-byte values are stored with the least significant byte first.
        \value ByteOrder The byte order of the platform the program was built for: BigEndian or LittleEndian.
    */
    enum Endian {
        BigEndian,
        LittleEndian,
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        ByteOrder = BigEndian
#else
        ByteOrder = LittleEndian
#endif
    };

    static QString buildCpuArchitecture();
    static QString currentCpuArchitecture();
    static QString buildAbi();
    static QString kernelType();
    static QString kernelVersion();
    static QString productType();
    static QString productVersion();
    static QString prettyProductName();
    static QString machineHostName();
    static QByteArray machineUniqueId();
    static QByteArray bootUniqueId();
};
