// MIT License - see LICENSE
#include "qdevice_p.h"

#include <cerrno>
#include <cstring>

/*!
    \module NtDevice
    \title Nt Device C++ Classes
    \brief Classes that look after an embedded device: its watchdog and its firmware.
    \cmaketarget Nt::Device

    Nt Device is a module of Nt only. QWatchdog arms the hardware watchdog of the system, which
    resets it when the application stops feeding it, and QFirmwareUpdate writes a new firmware
    image for the MCUboot boot loader of a Zephyr board and confirms it after the reboot. On Linux
    they use \c{/dev/watchdog} and a file or partition.

    See \l{Using Nt Device} for a walk through.
*/

namespace QtDevicePrivate {

void setFailure(Failure &failure, int code, const QString &what) {
    if (code < 0)
        code = -code;
    Error error = Error::Unknown;
    switch (code) {
    case ENOENT:
    case ENODEV:
    case ENXIO:
        error = Error::DeviceNotFound;
        break;
    case EACCES:
    case EPERM:
    case EROFS:
        error = Error::Permission;
        break;
    case EBUSY:
        error = Error::Busy;
        break;
    case ENOTSUP:
    case ENOSYS:
    case ENOTTY:
    case EINVAL:
        error = Error::Unsupported;
        break;
    case ENOMEM:
    case ENOSPC:
    case EMFILE:
    case ENFILE:
    case EFBIG:
        error = Error::Resource;
        break;
    case ETIMEDOUT:
        error = Error::Timeout;
        break;
    case EIO:
        error = Error::Io;
        break;
    default:
        break;
    }
    failure.error = error;
    failure.message = what + QStringLiteral(": ") + QString::fromUtf8(std::strerror(code));
}

void setFailure(Failure &failure, Error error, const QString &message) {
    failure.error = error;
    failure.message = message;
}

namespace {

quint32 readU32(const QByteArray &d, int at) {
    return static_cast<quint32>(static_cast<quint8>(d[at])) |
           (static_cast<quint32>(static_cast<quint8>(d[at + 1])) << 8) |
           (static_cast<quint32>(static_cast<quint8>(d[at + 2])) << 16) |
           (static_cast<quint32>(static_cast<quint8>(d[at + 3])) << 24);
}

quint16 readU16(const QByteArray &d, int at) {
    return static_cast<quint16>(static_cast<quint8>(d[at]) | (static_cast<quint8>(d[at + 1]) << 8));
}

WatchdogBackendFactory g_watchdogFactory = nullptr;
FirmwareBackendFactory g_firmwareFactory = nullptr;

} // namespace

// struct image_header of MCUboot, little-endian: magic, load address, header size (16 bits), protected TLV size
// (16 bits), image size, flags, then the version: major and minor (8 bits each), revision (16) and build (32).
bool parseImageHeader(const QByteArray &header, ImageInfo &info) {
    if (header.size() < kImageHeaderSize || readU32(header, 0) != kImageMagic)
        return false;
    info.headerSize = readU16(header, 8);
    info.imageSize = readU32(header, 12);
    const int major = static_cast<quint8>(header[20]);
    const int minor = static_cast<quint8>(header[21]);
    const int revision = readU16(header, 22);
    const auto build = static_cast<int>(readU32(header, 24));
    info.version = build ? QVersionNumber({major, minor, revision, build}) : QVersionNumber(major, minor, revision);
    return info.headerSize >= kImageHeaderSize;
}

void setWatchdogBackendFactory(WatchdogBackendFactory factory) { g_watchdogFactory = factory; }
void setFirmwareBackendFactory(FirmwareBackendFactory factory) { g_firmwareFactory = factory; }

std::unique_ptr<WatchdogBackend> createWatchdogBackend() {
    return g_watchdogFactory ? g_watchdogFactory() : createPlatformWatchdogBackend();
}
std::unique_ptr<FirmwareBackend> createFirmwareBackend() {
    return g_firmwareFactory ? g_firmwareFactory() : createPlatformFirmwareBackend();
}

} // namespace QtDevicePrivate
