// MIT License - see LICENSE
//
// QPwmChannel, QAnalogInput, QAnalogOutput, QI2cDevice and QSpiDevice on Zephyr. Devices are looked
// up with device_get_binding() and driven through the Zephyr driver APIs in the calling thread;
// nothing runs in an interrupt. CMakeLists.txt sets NT_HAVE_PWM, NT_HAVE_ADC, NT_HAVE_DAC,
// NT_HAVE_I2C and NT_HAVE_SPI from CONFIG_PWM and the others; without one, the class fails open()
// with UnsupportedOperationError.
#include "qperipheral_p.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if NT_HAVE_PWM
#include <zephyr/drivers/pwm.h>
#endif
#if NT_HAVE_ADC
#include <zephyr/drivers/adc.h>
#endif
#if NT_HAVE_DAC
#include <zephyr/drivers/dac.h>
#endif
#if NT_HAVE_I2C
#include <zephyr/drivers/i2c.h>
#endif
#if NT_HAVE_SPI
#include <zephyr/drivers/spi.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstring>

namespace QtPrivate {

namespace {

[[maybe_unused]] const device *bindDevice(const QString &name, const char *kind, HwFailure &failure) {
    const QByteArray bytes = name.toUtf8();
    const device *dev = device_get_binding(bytes.constData());
    if (!dev || !device_is_ready(dev)) {
        setHwFailure(failure, HwError::DeviceNotFound,
                     QStringLiteral("No %1 device is called %2").arg(QLatin1String(kind), name));
        return nullptr;
    }
    return dev;
}

// "device:channel", with a channel below limit.
[[maybe_unused]] const device *bindChannel(const QString &name, const char *kind, uint limit, uint &channel,
                                           HwFailure &failure) {
    const qsizetype colon = name.lastIndexOf(QLatin1Char(':'));
    bool ok = false;
    channel = colon > 0 ? name.mid(colon + 1).toUInt(&ok) : 0;
    if (!ok || channel >= limit) {
        setHwFailure(failure, HwError::DeviceNotFound,
                     QStringLiteral("%1 is not device:channel, with a channel below %2").arg(name).arg(limit));
        return nullptr;
    }
    return bindDevice(name.left(colon), kind, failure);
}

[[maybe_unused]] bool notConfigured(const char *option, HwFailure &failure) {
    setHwFailure(failure, HwError::Unsupported,
                 QStringLiteral("Zephyr was built without %1").arg(QLatin1String(option)));
    return false;
}

// --- PWM -----------------------------------------------------------------------------------------

#if NT_HAVE_PWM
class ZephyrPwmBackend final : public PwmBackend {
public:
    bool open(const QString &name, HwFailure &failure) override {
        m_device = bindChannel(name, "PWM", UINT32_MAX, m_channel, failure);
        return m_device != nullptr;
    }

    bool apply(const PwmSettings &s, HwFailure &failure) override {
        if (s.period > static_cast<qint64>(UINT32_MAX)) {
            setHwFailure(failure, HwError::Unsupported, QStringLiteral("A PWM period on Zephyr is at most 4.29 s"));
            return false;
        }
        if (s.period <= 0) {
            if (!s.enabled)
                return true; // nothing to program yet
            setHwFailure(failure, HwError::Unsupported, QStringLiteral("A PWM channel needs a period to be enabled"));
            return false;
        }
        // Zephyr has no enable: a channel that is off is driven with no pulse.
        const uint32_t pulse = s.enabled ? static_cast<uint32_t>(s.pulseWidth) : 0;
        const auto flags = static_cast<pwm_flags_t>(s.inversed ? PWM_POLARITY_INVERTED : PWM_POLARITY_NORMAL);
        const int result = pwm_set(m_device, m_channel, static_cast<uint32_t>(s.period), pulse, flags);
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("Cannot program PWM channel %1").arg(m_channel), false);
            return false;
        }
        return true;
    }

private:
    const device *m_device = nullptr;
    uint m_channel = 0;
};
#else
class ZephyrPwmBackend final : public PwmBackend {
public:
    bool open(const QString &, HwFailure &failure) override { return notConfigured("CONFIG_PWM", failure); }
    bool apply(const PwmSettings &, HwFailure &failure) override { return notConfigured("CONFIG_PWM", failure); }
};
#endif

// --- ADC -----------------------------------------------------------------------------------------

#if NT_HAVE_ADC
class ZephyrAnalogInputBackend final : public AnalogInputBackend {
public:
    bool open(const AnalogConfig &config, HwFailure &failure) override {
        m_device = bindChannel(config.name, "ADC", 32, m_channel, failure);
        if (!m_device)
            return false;
        m_resolution = static_cast<uint8_t>(config.resolution);
        adc_channel_cfg cfg{};
        cfg.gain = ADC_GAIN_1;
        cfg.reference = ADC_REF_INTERNAL;
        cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
        cfg.channel_id = static_cast<uint8_t>(m_channel & 0x1fU);
#if NT_HAVE_ADC_CONFIGURABLE_INPUTS
        cfg.input_positive = static_cast<uint8_t>(m_channel);
#endif
        const int result = adc_channel_setup(m_device, &cfg);
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("Cannot set up ADC channel %1").arg(m_channel), true);
            return false;
        }
        const uint16_t internal = adc_ref_internal(m_device);
        m_referenceVolts = internal > 0 ? internal / 1000.0 : config.referenceVolts;
        return true;
    }

    bool read(qint32 &raw, double &volts, bool &hasVolts, HwFailure &failure) override {
        uint32_t buffer = 0;
        adc_sequence sequence{};
        sequence.channels = static_cast<uint32_t>(BIT(m_channel));
        sequence.buffer = &buffer;
        sequence.buffer_size = sizeof buffer;
        sequence.resolution = m_resolution;
        const int result = adc_read(m_device, &sequence);
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("Cannot read ADC channel %1").arg(m_channel), false);
            return false;
        }
        // Drivers store samples of up to 16 bits as uint16_t, wider ones as uint32_t.
        if (m_resolution <= 16) {
            uint16_t sample = 0;
            std::memcpy(&sample, &buffer, sizeof sample);
            raw = sample;
        } else {
            raw = static_cast<qint32>(buffer);
        }
        hasVolts = m_referenceVolts > 0;
        volts = hasVolts ? raw * m_referenceVolts / static_cast<double>(1U << m_resolution) : 0.0;
        return true;
    }

private:
    const device *m_device = nullptr;
    uint m_channel = 0;
    uint8_t m_resolution = 12;
    double m_referenceVolts = 0;
};
#else
class ZephyrAnalogInputBackend final : public AnalogInputBackend {
public:
    bool open(const AnalogConfig &, HwFailure &failure) override { return notConfigured("CONFIG_ADC", failure); }
    bool read(qint32 &, double &, bool &, HwFailure &failure) override { return notConfigured("CONFIG_ADC", failure); }
};
#endif

// --- DAC -----------------------------------------------------------------------------------------

#if NT_HAVE_DAC
class ZephyrAnalogOutputBackend final : public AnalogOutputBackend {
public:
    bool open(const AnalogConfig &config, HwFailure &failure) override {
        m_device = bindChannel(config.name, "DAC", 256, m_channel, failure);
        if (!m_device)
            return false;
        m_resolution = config.resolution;
        m_referenceVolts = config.referenceVolts;
        dac_channel_cfg cfg{};
        cfg.channel_id = static_cast<uint8_t>(m_channel);
        cfg.resolution = static_cast<uint8_t>(m_resolution);
        cfg.buffered = true;
        const int result = dac_channel_setup(m_device, &cfg);
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("Cannot set up DAC channel %1").arg(m_channel), true);
            return false;
        }
        return true;
    }

    bool write(qint32 raw, HwFailure &failure) override {
        const int result = dac_write_value(m_device, static_cast<uint8_t>(m_channel), static_cast<uint32_t>(raw));
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("Cannot write DAC channel %1").arg(m_channel), false);
            return false;
        }
        return true;
    }

    bool rawForVolts(double volts, qint32 &raw) const override {
        if (!(m_referenceVolts > 0) || volts > m_referenceVolts)
            return false;
        const double steps = static_cast<double>(1U << m_resolution);
        const double value = std::round(volts / m_referenceVolts * steps);
        raw = static_cast<qint32>(value >= steps ? steps - 1 : value);
        return true;
    }

private:
    const device *m_device = nullptr;
    uint m_channel = 0;
    int m_resolution = 12;
    double m_referenceVolts = 0;
};
#else
class ZephyrAnalogOutputBackend final : public AnalogOutputBackend {
public:
    bool open(const AnalogConfig &, HwFailure &failure) override { return notConfigured("CONFIG_DAC", failure); }
    bool write(qint32, HwFailure &failure) override { return notConfigured("CONFIG_DAC", failure); }
    bool rawForVolts(double, qint32 &) const override { return false; }
};
#endif

// --- I2C -----------------------------------------------------------------------------------------

#if NT_HAVE_I2C
class ZephyrI2cBackend final : public I2cBackend {
public:
    bool open(const QString &bus, HwFailure &failure) override {
        m_device = bindDevice(bus, "I2C", failure);
        return m_device != nullptr;
    }

    bool transfer(quint16 address, const QByteArray &data, qsizetype readSize, QByteArray &read,
                  HwFailure &failure) override {
        if (readSize > 0)
            read.resize(readSize);
        const auto *out = reinterpret_cast<const uint8_t *>(data.constData());
        auto *in = reinterpret_cast<uint8_t *>(read.data());
        int result = 0;
        if (!data.isEmpty() && readSize > 0)
            result = i2c_write_read(m_device, address, out, static_cast<size_t>(data.size()), in,
                                    static_cast<size_t>(readSize));
        else if (!data.isEmpty())
            result = i2c_write(m_device, out, static_cast<uint32_t>(data.size()), address);
        else
            result = i2c_read(m_device, in, static_cast<uint32_t>(readSize), address);
        if (result < 0) {
            setHwFailure(failure, -result,
                         QStringLiteral("I2C transfer to 0x%1 failed").arg(address, 2, 16, QLatin1Char('0')), false);
            return false;
        }
        return true;
    }

private:
    const device *m_device = nullptr;
};
#else
class ZephyrI2cBackend final : public I2cBackend {
public:
    bool open(const QString &, HwFailure &failure) override { return notConfigured("CONFIG_I2C", failure); }
    bool transfer(quint16, const QByteArray &, qsizetype, QByteArray &, HwFailure &failure) override {
        return notConfigured("CONFIG_I2C", failure);
    }
};
#endif

// --- SPI -----------------------------------------------------------------------------------------

#if NT_HAVE_SPI
class ZephyrSpiBackend final : public SpiBackend {
public:
    bool open(const SpiConfig &config, HwFailure &failure) override {
        m_device = bindChannel(config.name, "SPI", 0x10000, m_slave, failure);
        return m_device != nullptr && configure(config, failure);
    }

    bool configure(const SpiConfig &config, HwFailure &) override {
        // Drivers recognise a configuration they have applied by its address, so a changed one
        // must live somewhere else; the new one is made while the old one still exists.
        auto next = std::make_unique<spi_config>();
        next->frequency = config.speed;
        uint32_t operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(static_cast<uint32_t>(config.bitsPerWord));
        if (config.mode & 2)
            operation |= SPI_MODE_CPOL;
        if (config.mode & 1)
            operation |= SPI_MODE_CPHA;
        operation |= config.lsbFirst ? SPI_TRANSFER_LSB : SPI_TRANSFER_MSB;
        next->operation = static_cast<spi_operation_t>(operation);
        next->slave = static_cast<uint16_t>(m_slave);
        m_config = std::move(next);
        return true; // the driver checks the configuration at the next transfer
    }

    bool transfer(const QByteArray &data, qsizetype readSize, bool duplex, QByteArray &read,
                  HwFailure &failure) override {
        QByteArray out = data; // spi_buf takes a non-const pointer
        spi_buf txBuffer{};
        spi_buf rxBuffers[2] = {};
        spi_buf_set tx{};
        spi_buf_set rx{};
        txBuffer.buf = out.data();
        txBuffer.len = static_cast<size_t>(out.size());
        tx.buffers = &txBuffer;
        tx.count = 1;
        rx.buffers = rxBuffers;
        if (duplex) {
            read.resize(out.size());
            rxBuffers[0].buf = read.data();
            rxBuffers[0].len = static_cast<size_t>(read.size());
            rx.count = 1;
        } else if (readSize > 0) {
            read = QByteArray(readSize, '\0');
            size_t count = 0;
            if (!out.isEmpty()) {
                rxBuffers[count].buf = nullptr; // skip what comes in while data goes out
                rxBuffers[count].len = static_cast<size_t>(out.size());
                ++count;
            }
            rxBuffers[count].buf = read.data();
            rxBuffers[count].len = static_cast<size_t>(readSize);
            rx.count = count + 1;
        }
        const int result =
            spi_transceive(m_device, m_config.get(), out.isEmpty() ? nullptr : &tx, rx.count > 0 ? &rx : nullptr);
        if (result < 0) {
            setHwFailure(failure, -result, QStringLiteral("SPI transfer failed"), false);
            return false;
        }
        return true;
    }

private:
    const device *m_device = nullptr;
    uint m_slave = 0;
    std::unique_ptr<spi_config> m_config;
};
#else
class ZephyrSpiBackend final : public SpiBackend {
public:
    bool open(const SpiConfig &, HwFailure &failure) override { return notConfigured("CONFIG_SPI", failure); }
    bool configure(const SpiConfig &, HwFailure &failure) override { return notConfigured("CONFIG_SPI", failure); }
    bool transfer(const QByteArray &, qsizetype, bool, QByteArray &, HwFailure &failure) override {
        return notConfigured("CONFIG_SPI", failure);
    }
};
#endif

} // namespace

std::unique_ptr<PwmBackend> createPlatformPwmBackend() { return std::make_unique<ZephyrPwmBackend>(); }
std::unique_ptr<AnalogInputBackend> createPlatformAnalogInputBackend() {
    return std::make_unique<ZephyrAnalogInputBackend>();
}
std::unique_ptr<AnalogOutputBackend> createPlatformAnalogOutputBackend() {
    return std::make_unique<ZephyrAnalogOutputBackend>();
}
std::unique_ptr<I2cBackend> createPlatformI2cBackend() { return std::make_unique<ZephyrI2cBackend>(); }
std::unique_ptr<SpiBackend> createPlatformSpiBackend() { return std::make_unique<ZephyrSpiBackend>(); }

} // namespace QtPrivate
