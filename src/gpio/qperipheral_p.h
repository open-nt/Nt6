// MIT License - see LICENSE
//
// The platform part of QPwmChannel, QAnalogInput, QAnalogOutput, QI2cDevice and QSpiDevice. The
// Linux backends live in src/gpio/q*backend_linux.cpp (sysfs for PWM and IIO, /dev/i2c-N and
// /dev/spidevB.C for the buses) and the Zephyr ones in src/zephyr/qperipheralbackend_zephyr.cpp;
// each defines the create*Backend() functions. The tests replace them with set*BackendFactory().
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <memory>

namespace QtPrivate {

// The errors of the peripheral classes. Each class has an enum of its own with the same values
// in the same order, which qperipheral.cpp checks with static_assert.
enum class HwError {
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

struct HwFailure {
    HwError error = HwError::NoError;
    QString message;
};

// Fills failure from the errno value code, with what as the start of the message. While opening,
// a missing device is DeviceNotFound; afterwards ENODEV means the device went away (Resource) and
// ENXIO or EREMOTEIO that a device on a bus did not answer (Io).
void setHwFailure(HwFailure &failure, int code, const QString &what, bool opening);
void setHwFailure(HwFailure &failure, HwError error, const QString &message);

// The directory that stands for /sys in the Linux PWM and IIO backends; the tests point it at a
// tree of their own. Empty means /sys.
QString sysfsRoot();
void setSysfsRoot(const QString &root);

// --- PWM -----------------------------------------------------------------------------------------

struct PwmSettings {
    qint64 period = 0; // nanoseconds
    qint64 pulseWidth = 0;
    bool inversed = false;
    bool enabled = false;
};

class PwmBackend {
public:
    PwmBackend() = default;
    virtual ~PwmBackend() = default;
    PwmBackend(const PwmBackend &) = delete;
    PwmBackend &operator=(const PwmBackend &) = delete;

    virtual bool open(const QString &name, HwFailure &failure) = 0;
    // Programs the channel. The backend knows what it last applied and writes in an order the
    // hardware accepts.
    virtual bool apply(const PwmSettings &settings, HwFailure &failure) = 0;
};

// --- Analog input and output ---------------------------------------------------------------------

struct AnalogConfig {
    QString name;
    int resolution = 12;       // bits; used where the driver must be told (Zephyr)
    double referenceVolts = 0; // 0 when unknown
};

class AnalogInputBackend {
public:
    AnalogInputBackend() = default;
    virtual ~AnalogInputBackend() = default;
    AnalogInputBackend(const AnalogInputBackend &) = delete;
    AnalogInputBackend &operator=(const AnalogInputBackend &) = delete;

    virtual bool open(const AnalogConfig &config, HwFailure &failure) = 0;
    // Takes one sample. hasVolts is false when the raw value cannot be converted.
    virtual bool read(qint32 &raw, double &volts, bool &hasVolts, HwFailure &failure) = 0;
};

class AnalogOutputBackend {
public:
    AnalogOutputBackend() = default;
    virtual ~AnalogOutputBackend() = default;
    AnalogOutputBackend(const AnalogOutputBackend &) = delete;
    AnalogOutputBackend &operator=(const AnalogOutputBackend &) = delete;

    virtual bool open(const AnalogConfig &config, HwFailure &failure) = 0;
    virtual bool write(qint32 raw, HwFailure &failure) = 0;
    // Converts volts to the raw value that comes closest; false if the scale is unknown.
    virtual bool rawForVolts(double volts, qint32 &raw) const = 0;
};

// --- I2C -----------------------------------------------------------------------------------------

class I2cBackend {
public:
    I2cBackend() = default;
    virtual ~I2cBackend() = default;
    I2cBackend(const I2cBackend &) = delete;
    I2cBackend &operator=(const I2cBackend &) = delete;

    virtual bool open(const QString &bus, HwFailure &failure) = 0;
    // Writes data to the device at address (if data is not empty), then reads readSize bytes
    // into read (if readSize is not 0), with a repeated start in between: one bus transaction.
    virtual bool transfer(quint16 address, const QByteArray &data, qsizetype readSize, QByteArray &read,
                          HwFailure &failure) = 0;
};

// --- SPI -----------------------------------------------------------------------------------------

struct SpiConfig {
    QString name;
    int mode = 0; // 0 to 3: CPOL is bit 1, CPHA bit 0
    quint32 speed = 1000000;
    int bitsPerWord = 8;
    bool lsbFirst = false;
};

class SpiBackend {
public:
    SpiBackend() = default;
    virtual ~SpiBackend() = default;
    SpiBackend(const SpiBackend &) = delete;
    SpiBackend &operator=(const SpiBackend &) = delete;

    virtual bool open(const SpiConfig &config, HwFailure &failure) = 0;
    // Applies a changed configuration to the open device.
    virtual bool configure(const SpiConfig &config, HwFailure &failure) = 0;
    // One transaction with chip select held. With duplex, data is clocked out and as many bytes
    // are clocked in, into read. Otherwise data is sent, then readSize bytes are read into read.
    virtual bool transfer(const QByteArray &data, qsizetype readSize, bool duplex, QByteArray &read,
                          HwFailure &failure) = 0;
};

// --- Factories -----------------------------------------------------------------------------------

std::unique_ptr<PwmBackend> createPlatformPwmBackend();
std::unique_ptr<AnalogInputBackend> createPlatformAnalogInputBackend();
std::unique_ptr<AnalogOutputBackend> createPlatformAnalogOutputBackend();
std::unique_ptr<I2cBackend> createPlatformI2cBackend();
std::unique_ptr<SpiBackend> createPlatformSpiBackend();

using PwmBackendFactory = std::unique_ptr<PwmBackend> (*)();
using AnalogInputBackendFactory = std::unique_ptr<AnalogInputBackend> (*)();
using AnalogOutputBackendFactory = std::unique_ptr<AnalogOutputBackend> (*)();
using I2cBackendFactory = std::unique_ptr<I2cBackend> (*)();
using SpiBackendFactory = std::unique_ptr<SpiBackend> (*)();

// Make the classes use factory, or the platform backend again when it is null.
void setPwmBackendFactory(PwmBackendFactory factory);
void setAnalogInputBackendFactory(AnalogInputBackendFactory factory);
void setAnalogOutputBackendFactory(AnalogOutputBackendFactory factory);
void setI2cBackendFactory(I2cBackendFactory factory);
void setSpiBackendFactory(SpiBackendFactory factory);

std::unique_ptr<PwmBackend> createPwmBackend();
std::unique_ptr<AnalogInputBackend> createAnalogInputBackend();
std::unique_ptr<AnalogOutputBackend> createAnalogOutputBackend();
std::unique_ptr<I2cBackend> createI2cBackend();
std::unique_ptr<SpiBackend> createSpiBackend();

} // namespace QtPrivate
