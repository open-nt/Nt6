// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtCore/qlogging.h>
#include <QtGpio/qanaloginput.h>
#include <QtGpio/qanalogoutput.h>
#include <QtGpio/qi2cdevice.h>
#include <QtGpio/qpwmchannel.h>
#include <QtGpio/qspidevice.h>

#include <cerrno>

/*!
    \page nt-gpio-peripherals.html
    \title Using PWM, Analog I/O, I2C and SPI
    \brief Naming the channels and buses of a board, and driving them on Linux and on Zephyr.

    Besides QGpioLine, Nt GPIO has a class for each of the other peripherals that an embedded
    board brings out:

    \table
    \header
        \li Class
        \li Peripheral
        \li Linux
        \li Zephyr
    \row
        \li QPwmChannel
        \li A pulse-width modulation output
        \li \c /sys/class/pwm
        \li \c pwm_set(), with \c CONFIG_PWM
    \row
        \li QAnalogInput
        \li An analog to digital converter channel
        \li Industrial I/O, \c /sys/bus/iio
        \li \c adc_read(), with \c CONFIG_ADC
    \row
        \li QAnalogOutput
        \li A digital to analog converter channel
        \li Industrial I/O, \c /sys/bus/iio
        \li \c dac_write_value(), with \c CONFIG_DAC
    \row
        \li QI2cDevice
        \li One device on an I2C bus
        \li \c /dev/i2c-N
        \li \c i2c_write_read(), with \c CONFIG_I2C
    \row
        \li QSpiDevice
        \li One device on an SPI bus
        \li \c /dev/spidevB.C
        \li \c spi_transceive(), with \c CONFIG_SPI
    \endtable

    All of them work the same way: give the object a name, set it up, call open(), and check
    error() and errorString() when a call returns \c false or an empty QByteArray. The calls
    are synchronous: a transfer on a bus or a conversion of an ADC returns when it is done, in
    the thread that makes it. They take microseconds to a few milliseconds; a program that
    talks to slow devices in bulk does so from a worker thread.

    Like QGpioLine, these classes are not part of Qt.

    \section1 Naming Channels and Buses

    \table
    \header
        \li Class
        \li Linux
        \li Zephyr
    \row
        \li QPwmChannel
        \li \c pwmchip0:1, channel 1 of \c /sys/class/pwm/pwmchip0
        \li \c pwm@40018000:1, channel 1 of that device
    \row
        \li QAnalogInput, QAnalogOutput
        \li \c iio:device0:2, channel 2 of that IIO device, or \c ads1015:2 to find the device
            by the name in its \c name file. A channel that is not a number, such as \c temp,
            names \c in_temp_raw.
        \li \c adc@40066000:2, channel 2 of that device
    \row
        \li QI2cDevice
        \li \c i2c-1 or \c /dev/i2c-1, and an address
        \li \c i2c@40044000, and an address
    \row
        \li QSpiDevice
        \li \c spidev0.1 or \c /dev/spidev0.1, bus 0 and chip select 1
        \li \c spi@40008000:1, chip select 1 of that controller
    \endtable

    On Zephyr, the device part is the name that \c device_get_binding() takes: the devicetree
    node name, or its \c label.

    \section1 PWM

    The period and the pulse width are in nanoseconds. Set them before open(), or while the
    channel is open, where they take effect at once:

    \code
    QPwmChannel backlight(QStringLiteral("pwmchip0:0"));
    backlight.setFrequency(20000);  // 20 kHz, above hearing
    backlight.setDutyCycle(0.6);
    backlight.setEnabled(true);
    if (!backlight.open())
        qWarning() << backlight.errorString();
    \endcode

    On Linux, open() exports the channel if it is not exported yet, and close() unexports it
    again. The pulse width is kept within the period whatever order the two are set in.

    \section1 Analog Input

    read() takes one sample; with a sample interval, the input is read from the event loop and
    valueChanged() reports each change:

    \code
    QAnalogInput battery(QStringLiteral("iio:device0:3"));
    battery.setSampleInterval(500);
    QObject::connect(&battery, &QAnalogInput::valueChanged, [&] {
        qDebug() << "battery" << battery.voltage() << "V";
    });
    battery.open();
    \endcode

    On Linux, rawValue() is converted to volts with the \c scale and \c offset attributes of the
    channel. On Zephyr, the channel is set up at open() with a gain of 1, the internal reference
    and the default acquisition time, at resolution() bits, and converted with the reference
    voltage that the driver reports, or referenceVoltage() if it reports none.

    \section1 Analog Output

    \code
    QAnalogOutput bias(QStringLiteral("iio:device1:0"));
    if (bias.open())
        bias.setVoltage(1.25);
    \endcode

    On Zephyr, setVoltage() needs referenceVoltage(), since a DAC driver does not report it.

    \section1 I2C

    \code
    QI2cDevice sensor(QStringLiteral("i2c-1"), 0x48);
    if (!sensor.open())
        qFatal("%s", qPrintable(sensor.errorString()));
    const QByteArray t = sensor.readRegister(0x00, 2); // a repeated start between the two parts
    if (t.isEmpty() && sensor.error() == QI2cDevice::IoError)
        qWarning() << "no answer from 0x48";
    \endcode

    A device that does not answer is an \c IoError; the bus stays open. Addresses are 7-bit.
    On Linux, the adapter must support plain I2C transfers (\c I2C_FUNC_I2C); an adapter that
    only speaks SMBus fails open() with \c UnsupportedOperationError.

    \section1 SPI

    \code
    QSpiDevice flash(QStringLiteral("spidev0.0"));
    flash.setMode(QSpiDevice::Mode0);
    flash.setSpeed(10000000);
    flash.open();
    const QByteArray id = flash.writeRead(QByteArray("\x9f", 1), 3); // JEDEC ID
    \endcode

    transfer() clocks bytes out and in at the same time; writeRead() sends bytes, then reads,
    with chip select held across both. On Linux, \c spidev must be bound to the device, through
    the devicetree or \c driver_override, and one transfer is limited by its \c bufsiz parameter,
    4096 bytes by default.

    \section1 Zephyr

    Set \c CONFIG_NT_GPIO, and the Zephyr option of each peripheral used, in \c prj.conf. A
    class whose peripheral is not configured fails open() with \c UnsupportedOperationError.

    \code
    CONFIG_NT=y
    CONFIG_NT_GPIO=y
    CONFIG_PWM=y
    CONFIG_ADC=y
    CONFIG_I2C=y
    CONFIG_SPI=y
    \endcode

    The calls go straight to the Zephyr driver in the calling thread; nothing runs in an
    interrupt. Settings that Zephyr keeps in the devicetree specs (\c pwm_dt_spec,
    \c adc_dt_spec, \c spi_dt_spec) are taken from the object instead: the period of a PWM
    channel, the resolution and reference of an ADC channel, and the mode and speed of an SPI
    device. An SPI device is selected by the native chip select of the controller; a chip select
    on a GPIO pin, from \c cs-gpios, is not used.

    \sa QGpioLine, {Using Nt GPIO}, {Nt on Zephyr}
*/

namespace QtPrivate {

#define NT_CHECK_ERRORS(Class, Enum)                                                                                   \
    static_assert(int(Class::NoError) == int(HwError::NoError) && int(Class::UnknownError) == int(HwError::Unknown) && \
                      int(Class::IoError) == int(HwError::Io) && int(Class::TimeoutError) == int(HwError::Timeout),    \
                  #Class "::" #Enum " must list the values of HwError in its order")
NT_CHECK_ERRORS(QPwmChannel, PwmError);
NT_CHECK_ERRORS(QAnalogInput, AnalogError);
NT_CHECK_ERRORS(QAnalogOutput, AnalogError);
NT_CHECK_ERRORS(QI2cDevice, I2cError);
NT_CHECK_ERRORS(QSpiDevice, SpiError);
#undef NT_CHECK_ERRORS

void setHwFailure(HwFailure &failure, HwError error, const QString &message) {
    failure.error = error;
    failure.message = message;
}

void setHwFailure(HwFailure &failure, int code, const QString &what, bool opening) {
    switch (code) {
    case ENOENT:
        failure.error = HwError::DeviceNotFound;
        break;
    case ENODEV:
#ifdef ESHUTDOWN
    case ESHUTDOWN:
#endif
        failure.error = opening ? HwError::DeviceNotFound : HwError::Resource;
        break;
    case ENXIO:
        failure.error = opening ? HwError::DeviceNotFound : HwError::Io;
        break;
    case ENOTTY: // the file is not the kind of device that was asked for
        failure.error = opening ? HwError::DeviceNotFound : HwError::Unsupported;
        break;
    case EIO:
#ifdef EREMOTEIO
    case EREMOTEIO:
#endif
        failure.error = HwError::Io;
        break;
    case EACCES:
    case EPERM:
        failure.error = HwError::Permission;
        break;
    case EBUSY:
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        failure.error = HwError::Busy;
        break;
    case ETIMEDOUT:
        failure.error = HwError::Timeout;
        break;
    case EINVAL:
    case EMSGSIZE:
    case ENOSYS:
    case ENOTSUP:
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
    case EOPNOTSUPP:
#endif
        failure.error = HwError::Unsupported;
        break;
    default:
        failure.error = HwError::Unknown;
        break;
    }
    failure.message = what + QStringLiteral(": ") + qt_error_string(code);
}

namespace {
QString g_sysfsRoot;
PwmBackendFactory g_pwmFactory = nullptr;
AnalogInputBackendFactory g_analogInputFactory = nullptr;
AnalogOutputBackendFactory g_analogOutputFactory = nullptr;
I2cBackendFactory g_i2cFactory = nullptr;
SpiBackendFactory g_spiFactory = nullptr;
} // namespace

QString sysfsRoot() { return g_sysfsRoot.isEmpty() ? QStringLiteral("/sys") : g_sysfsRoot; }
void setSysfsRoot(const QString &root) { g_sysfsRoot = root; }

void setPwmBackendFactory(PwmBackendFactory factory) { g_pwmFactory = factory; }
void setAnalogInputBackendFactory(AnalogInputBackendFactory factory) { g_analogInputFactory = factory; }
void setAnalogOutputBackendFactory(AnalogOutputBackendFactory factory) { g_analogOutputFactory = factory; }
void setI2cBackendFactory(I2cBackendFactory factory) { g_i2cFactory = factory; }
void setSpiBackendFactory(SpiBackendFactory factory) { g_spiFactory = factory; }

std::unique_ptr<PwmBackend> createPwmBackend() { return g_pwmFactory ? g_pwmFactory() : createPlatformPwmBackend(); }
std::unique_ptr<AnalogInputBackend> createAnalogInputBackend() {
    return g_analogInputFactory ? g_analogInputFactory() : createPlatformAnalogInputBackend();
}
std::unique_ptr<AnalogOutputBackend> createAnalogOutputBackend() {
    return g_analogOutputFactory ? g_analogOutputFactory() : createPlatformAnalogOutputBackend();
}
std::unique_ptr<I2cBackend> createI2cBackend() { return g_i2cFactory ? g_i2cFactory() : createPlatformI2cBackend(); }
std::unique_ptr<SpiBackend> createSpiBackend() { return g_spiFactory ? g_spiFactory() : createPlatformSpiBackend(); }

} // namespace QtPrivate
