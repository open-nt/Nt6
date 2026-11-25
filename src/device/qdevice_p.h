// MIT License - see LICENSE
//
// The platform part of QWatchdog and QFirmwareUpdate. Linux has src/device/qdevicebackend_linux.cpp (/dev/watchdog,
// and an image written to a file or partition); Zephyr has src/zephyr/qdevicebackend_zephyr.cpp (the watchdog
// drivers, MCUboot and hwinfo). The tests replace them with setWatchdogBackendFactory() and
// setFirmwareBackendFactory().
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qversionnumber.h>

#include <memory>

namespace QtDevicePrivate {

// The errors of QWatchdog and QFirmwareUpdate, in the order of their enums.
enum class Error {
    NoError,
    DeviceNotFound,
    Permission,
    Busy,
    Open,
    Unsupported,
    Resource,
    NotOpen,
    Timeout,
    Io,
    Unknown
};

struct Failure {
    Error error = Error::NoError;
    QString message;
};

// Fills failure from the errno value code (negative values are taken as -code, as Zephyr returns them).
void setFailure(Failure &failure, int code, const QString &what);
void setFailure(Failure &failure, Error error, const QString &message);

// --- Watchdog ------------------------------------------------------------------------------------

struct WatchdogConfig {
    QString name;
    int timeout = 0; // milliseconds
    bool pauseInDebugger = false;
    bool pauseWhileSleeping = false;
};

class WatchdogBackend {
public:
    WatchdogBackend() = default;
    virtual ~WatchdogBackend() = default;
    WatchdogBackend(const WatchdogBackend &) = delete;
    WatchdogBackend &operator=(const WatchdogBackend &) = delete;

    // Opens and arms the watchdog. timeout is set to what the hardware uses, which may be longer.
    virtual bool start(WatchdogConfig &config, Failure &failure) = 0;
    virtual bool feed(Failure &failure) = 0;
    // Disarms the watchdog; fails with Unsupported where the hardware cannot be stopped once started.
    virtual bool stop(Failure &failure) = 0;
    // Whether the watchdog reset the system the last time; false if that is not known.
    virtual bool causedLastReset(const QString &name) = 0;
};

// --- Firmware ------------------------------------------------------------------------------------

class FirmwareBackend {
public:
    FirmwareBackend() = default;
    virtual ~FirmwareBackend() = default;
    FirmwareBackend(const FirmwareBackend &) = delete;
    FirmwareBackend &operator=(const FirmwareBackend &) = delete;

    // Prepares the slot that takes the new image, erasing it if needed. size is -1 when unknown.
    virtual bool begin(const QString &target, qint64 size, Failure &failure) = 0;
    virtual bool write(const char *data, qsizetype size, Failure &failure) = 0;
    // Writes what is buffered and reads the first bytes of the slot back, for the header to be checked.
    virtual bool finish(QByteArray &header, Failure &failure) = 0;
    virtual void abort() = 0;
    // Asks the boot loader to boot the new image next, once (test) or for good (permanent).
    virtual bool requestUpgrade(bool permanent, Failure &failure) = 0;
    // The running image: whether it is confirmed, and confirming it so that it is not rolled back.
    virtual bool isConfirmed() = 0;
    virtual bool confirm(Failure &failure) = 0;
    // The header of the running image, or an empty array.
    virtual QByteArray runningHeader() = 0;
    virtual bool reboot(Failure &failure) = 0;
};

// The version and size in an MCUboot image header; false if header is not one.
struct ImageInfo {
    QVersionNumber version;
    quint32 headerSize = 0;
    quint32 imageSize = 0;
};
bool parseImageHeader(const QByteArray &header, ImageInfo &info);
inline constexpr quint32 kImageMagic = 0x96f3b83d;
inline constexpr int kImageHeaderSize = 32;

// --- Factories -----------------------------------------------------------------------------------

std::unique_ptr<WatchdogBackend> createPlatformWatchdogBackend();
std::unique_ptr<FirmwareBackend> createPlatformFirmwareBackend();

using WatchdogBackendFactory = std::unique_ptr<WatchdogBackend> (*)();
using FirmwareBackendFactory = std::unique_ptr<FirmwareBackend> (*)();

// Make the classes use factory, or the platform backend again when it is null.
void setWatchdogBackendFactory(WatchdogBackendFactory factory);
void setFirmwareBackendFactory(FirmwareBackendFactory factory);

std::unique_ptr<WatchdogBackend> createWatchdogBackend();
std::unique_ptr<FirmwareBackend> createFirmwareBackend();

} // namespace QtDevicePrivate
