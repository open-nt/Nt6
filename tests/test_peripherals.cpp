// MIT License - see LICENSE
//
// QPwmChannel, QAnalogInput, QAnalogOutput, QI2cDevice and QSpiDevice are tested against fake
// backends, for what the classes do with settings, errors and the event loop. The Linux PWM and
// IIO backends are tested for real against a tree of files that stands in for /sys; the I2C and
// SPI backends only for how they report devices that are missing or are something else, since a
// real bus needs hardware or root.
#include <QtCore>
#include <QtGpio>

#include "qperipheral_p.h"

#include <cerrno>
#include <cstdio>
#include <deque>

using QtPrivate::HwFailure;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> static bool waitFor(F cond, int timeoutMs = 2000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(2);
    return cond();
}

namespace {

// What the fake devices see and do; shared by the tests and the backends they create.
struct Fake {
    int failOpen = 0;     // an errno value open() fails with, or 0
    int failTransfer = 0; // an errno value the next operations fail with, or 0
    QString name;         // as passed to open()
    std::vector<QtPrivate::PwmSettings> applied;
    std::deque<qint32> samples;
    qint32 lastSample = 0;
    std::vector<qint32> written;
    quint16 address = 0;
    QByteArray sent;
    qsizetype readSize = 0;
    bool duplex = false;
    std::vector<QtPrivate::SpiConfig> configs;
};
Fake g_fake;

bool failed(int code, HwFailure &failure, bool opening) {
    if (code == 0)
        return false;
    QtPrivate::setHwFailure(failure, code, QStringLiteral("Fake failure"), opening);
    return true;
}

class FakePwm final : public QtPrivate::PwmBackend {
public:
    bool open(const QString &name, HwFailure &failure) override {
        g_fake.name = name;
        return !failed(g_fake.failOpen, failure, true);
    }
    bool apply(const QtPrivate::PwmSettings &s, HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        g_fake.applied.push_back(s);
        return true;
    }
};

class FakeAdc final : public QtPrivate::AnalogInputBackend {
public:
    bool open(const QtPrivate::AnalogConfig &config, HwFailure &failure) override {
        g_fake.name = config.name;
        return !failed(g_fake.failOpen, failure, true);
    }
    bool read(qint32 &raw, double &volts, bool &hasVolts, HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        if (!g_fake.samples.empty()) {
            g_fake.lastSample = g_fake.samples.front();
            g_fake.samples.pop_front();
        }
        raw = g_fake.lastSample;
        volts = raw / 1000.0;
        hasVolts = true;
        return true;
    }
};

class FakeDac final : public QtPrivate::AnalogOutputBackend {
public:
    bool open(const QtPrivate::AnalogConfig &config, HwFailure &failure) override {
        g_fake.name = config.name;
        return !failed(g_fake.failOpen, failure, true);
    }
    bool write(qint32 raw, HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        g_fake.written.push_back(raw);
        return true;
    }
    bool rawForVolts(double volts, qint32 &raw) const override {
        raw = static_cast<qint32>(std::lround(volts * 1000)); // 1 mV a step
        return volts <= 3.3;
    }
};

class FakeI2c final : public QtPrivate::I2cBackend {
public:
    bool open(const QString &bus, HwFailure &failure) override {
        g_fake.name = bus;
        return !failed(g_fake.failOpen, failure, true);
    }
    bool transfer(quint16 address, const QByteArray &data, qsizetype readSize, QByteArray &read,
                  HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        g_fake.address = address;
        g_fake.sent = data;
        g_fake.readSize = readSize;
        read.clear();
        for (qsizetype i = 0; i < readSize; ++i)
            read.append(static_cast<char>(0xa0 + i));
        return true;
    }
};

class FakeSpi final : public QtPrivate::SpiBackend {
public:
    bool open(const QtPrivate::SpiConfig &config, HwFailure &failure) override {
        g_fake.name = config.name;
        if (failed(g_fake.failOpen, failure, true))
            return false;
        g_fake.configs.push_back(config);
        return true;
    }
    bool configure(const QtPrivate::SpiConfig &config, HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        g_fake.configs.push_back(config);
        return true;
    }
    bool transfer(const QByteArray &data, qsizetype readSize, bool duplex, QByteArray &read,
                  HwFailure &failure) override {
        if (failed(g_fake.failTransfer, failure, false))
            return false;
        g_fake.sent = data;
        g_fake.readSize = readSize;
        g_fake.duplex = duplex;
        read.clear();
        const qsizetype n = duplex ? data.size() : readSize;
        for (qsizetype i = 0; i < n; ++i)
            read.append(duplex ? static_cast<char>(~data.at(i)) : static_cast<char>(0x50 + i));
        return true;
    }
};

std::unique_ptr<QtPrivate::PwmBackend> makePwm() { return std::make_unique<FakePwm>(); }
std::unique_ptr<QtPrivate::AnalogInputBackend> makeAdc() { return std::make_unique<FakeAdc>(); }
std::unique_ptr<QtPrivate::AnalogOutputBackend> makeDac() { return std::make_unique<FakeDac>(); }
std::unique_ptr<QtPrivate::I2cBackend> makeI2c() { return std::make_unique<FakeI2c>(); }
std::unique_ptr<QtPrivate::SpiBackend> makeSpi() { return std::make_unique<FakeSpi>(); }

void useFakes(bool on) {
    QtPrivate::setPwmBackendFactory(on ? makePwm : nullptr);
    QtPrivate::setAnalogInputBackendFactory(on ? makeAdc : nullptr);
    QtPrivate::setAnalogOutputBackendFactory(on ? makeDac : nullptr);
    QtPrivate::setI2cBackendFactory(on ? makeI2c : nullptr);
    QtPrivate::setSpiBackendFactory(on ? makeSpi : nullptr);
}

} // namespace

static void testPwm() {
    g_fake = Fake();
    QPwmChannel pwm;
    CHECK(pwm.period() == 0);
    CHECK(pwm.pulseWidth() == 0);
    CHECK(pwm.polarity() == QPwmChannel::Normal);
    CHECK(!pwm.isEnabled());
    CHECK(!pwm.isOpen());
    CHECK(!pwm.open());
    CHECK(pwm.error() == QPwmChannel::DeviceNotFoundError);
    CHECK(QPwmChannel::staticMetaObject.indexOfProperty("dutyCycle") >= 0);

    // The duty cycle may come first and is kept when the period is set and changed.
    pwm.setChannelName(QStringLiteral("pwmchip0:1"));
    CHECK(pwm.setDutyCycle(0.25));
    CHECK(pwm.setFrequency(1000));
    CHECK(pwm.period() == 1000000);
    CHECK(pwm.pulseWidth() == 250000);
    CHECK(qFuzzyCompare(pwm.frequency(), 1000.0));
    CHECK(pwm.setPeriod(2000000));
    CHECK(pwm.pulseWidth() == 500000);
    CHECK(g_fake.applied.empty());

    CHECK(pwm.setEnabled(true));
    CHECK(pwm.open());
    CHECK(g_fake.name == QStringLiteral("pwmchip0:1"));
    CHECK(g_fake.applied.size() == 1);
    CHECK(g_fake.applied.back().period == 2000000);
    CHECK(g_fake.applied.back().pulseWidth == 500000);
    CHECK(g_fake.applied.back().enabled);
    CHECK(!pwm.open());
    CHECK(pwm.error() == QPwmChannel::OpenError);

    // While open, each change is applied at once.
    CHECK(pwm.setPulseWidth(1500000));
    CHECK(qFuzzyCompare(pwm.dutyCycle(), 0.75));
    CHECK(g_fake.applied.back().pulseWidth == 1500000);
    CHECK(pwm.setPolarity(QPwmChannel::Inversed));
    CHECK(g_fake.applied.back().inversed);

    // Values out of range are refused and change nothing.
    const std::size_t applies = g_fake.applied.size();
    CHECK(!pwm.setPulseWidth(3000000));
    CHECK(pwm.error() == QPwmChannel::UnsupportedOperationError);
    CHECK(!pwm.setDutyCycle(1.5));
    CHECK(!pwm.setPeriod(-1));
    CHECK(!pwm.setFrequency(0));
    CHECK(g_fake.applied.size() == applies);
    CHECK(pwm.pulseWidth() == 1500000);

    // A setting the hardware refuses is not kept.
    g_fake.failTransfer = EINVAL;
    QList<QPwmChannel::PwmError> errors;
    QObject::connect(&pwm, &QPwmChannel::errorOccurred, [&](QPwmChannel::PwmError e) { errors.append(e); });
    CHECK(!pwm.setPeriod(10));
    CHECK(pwm.period() == 2000000);
    CHECK(pwm.error() == QPwmChannel::UnsupportedOperationError);
    CHECK(errors == QList<QPwmChannel::PwmError>{QPwmChannel::UnsupportedOperationError});
    CHECK(pwm.isOpen());

    // The device goes away.
    g_fake.failTransfer = ENODEV;
    CHECK(!pwm.setEnabled(false));
    CHECK(pwm.error() == QPwmChannel::ResourceError);
    CHECK(!pwm.isOpen());
    CHECK(pwm.isEnabled());

    g_fake = Fake();
    g_fake.failOpen = EBUSY;
    CHECK(!pwm.open());
    CHECK(pwm.error() == QPwmChannel::BusyError);
    pwm.clearError();
    CHECK(pwm.error() == QPwmChannel::NoError);
    CHECK(pwm.errorString().isEmpty());
}

static void testAnalogInput() {
    g_fake = Fake();
    QAnalogInput adc(QStringLiteral("iio:device0:2"));
    CHECK(adc.resolution() == 12);
    CHECK(adc.sampleInterval() == 0);
    adc.setResolution(40);
    CHECK(adc.resolution() == 24);
    adc.setResolution(12);
    CHECK(!adc.read());
    CHECK(adc.error() == QAnalogInput::NotOpenError);

    QList<int> changes;
    QObject::connect(&adc, &QAnalogInput::valueChanged, [&](int raw) { changes.append(raw); });
    g_fake.samples = {1200};
    CHECK(adc.open());
    CHECK(adc.error() == QAnalogInput::NoError);
    CHECK(adc.rawValue() == 1200);
    CHECK(adc.hasVoltage());
    CHECK(qFuzzyCompare(adc.voltage(), 1.2));
    CHECK(changes == QList<int>{1200});

    // A sample like the last one is not a change.
    CHECK(adc.read());
    CHECK(changes.size() == 1);

    // Sampling from the event loop.
    g_fake.samples = {1300, 1300, 1400};
    adc.setSampleInterval(5);
    CHECK(waitFor([&] { return changes.size() == 3; }));
    CHECK((changes == QList<int>{1200, 1300, 1400}));

    // The converter goes away: the input is closed and sampling stops.
    g_fake.failTransfer = ENODEV;
    QList<QAnalogInput::AnalogError> errors;
    QObject::connect(&adc, &QAnalogInput::errorOccurred, [&](QAnalogInput::AnalogError e) { errors.append(e); });
    CHECK(waitFor([&] { return !errors.isEmpty(); }));
    CHECK(errors.first() == QAnalogInput::ResourceError);
    CHECK(!adc.isOpen());
    spinEvents(20);
    CHECK(errors.size() == 1);

    // A conversion that times out leaves the input open.
    g_fake = Fake();
    adc.setSampleInterval(0);
    CHECK(adc.open());
    g_fake.failTransfer = ETIMEDOUT;
    CHECK(!adc.read());
    CHECK(adc.error() == QAnalogInput::TimeoutError);
    CHECK(adc.isOpen());
}

static void testAnalogOutput() {
    g_fake = Fake();
    QAnalogOutput dac(QStringLiteral("iio:device1:0"));
    CHECK(!dac.setVoltage(1.0));
    CHECK(dac.error() == QAnalogOutput::NotOpenError);
    CHECK(dac.setRawValue(100)); // kept for open()
    CHECK(g_fake.written.empty());
    CHECK(dac.open());
    CHECK((g_fake.written == std::vector<qint32>{100}));
    CHECK(dac.setVoltage(1.25));
    CHECK(dac.rawValue() == 1250);
    CHECK(g_fake.written.back() == 1250);
    CHECK(!dac.setVoltage(5.0));
    CHECK(dac.error() == QAnalogOutput::UnsupportedOperationError);
    CHECK(!dac.setRawValue(-1));
    CHECK(dac.rawValue() == 1250);
    g_fake.failTransfer = EIO;
    CHECK(!dac.setRawValue(7));
    CHECK(dac.error() == QAnalogOutput::IoError);
    CHECK(dac.rawValue() == 1250);
}

static void testI2c() {
    g_fake = Fake();
    QI2cDevice dev;
    CHECK(!dev.open());
    CHECK(dev.error() == QI2cDevice::DeviceNotFoundError);
    dev.setBusName(QStringLiteral("i2c-1"));
    dev.setAddress(0x80);
    CHECK(!dev.open());
    CHECK(dev.error() == QI2cDevice::UnsupportedOperationError);
    dev.setAddress(0x48);
    CHECK(dev.read(2).isEmpty());
    CHECK(dev.error() == QI2cDevice::NotOpenError);
    CHECK(dev.open());
    CHECK(g_fake.name == QStringLiteral("i2c-1"));

    CHECK(dev.write(QByteArray("\x01\x02", 2)));
    CHECK(g_fake.address == 0x48);
    CHECK(g_fake.sent == QByteArray("\x01\x02", 2));
    CHECK(g_fake.readSize == 0);

    CHECK(dev.read(2) == QByteArray("\xa0\xa1", 2));
    CHECK(g_fake.sent.isEmpty());

    CHECK(dev.readRegister(0x0f, 3) == QByteArray("\xa0\xa1\xa2", 3));
    CHECK(g_fake.sent == QByteArray(1, '\x0f'));
    CHECK(g_fake.readSize == 3);

    CHECK(dev.writeRegister(0x10, QByteArray("\x55", 1)));
    CHECK(g_fake.sent == QByteArray("\x10\x55", 2));

    // Talking to another device on the same bus.
    dev.setAddress(0x50);
    CHECK(dev.read(1).size() == 1);
    CHECK(g_fake.address == 0x50);

    CHECK(!dev.write(QByteArray()));
    CHECK(dev.error() == QI2cDevice::UnsupportedOperationError);
    CHECK(dev.read(-1).isEmpty());

    // No answer: an I/O error, and the bus stays open.
    g_fake.failTransfer = ENXIO;
    CHECK(dev.readRegister(0, 1).isEmpty());
    CHECK(dev.error() == QI2cDevice::IoError);
    CHECK(dev.isOpen());
    g_fake.failTransfer = ENODEV;
    CHECK(!dev.write(QByteArray(1, 'x')));
    CHECK(dev.error() == QI2cDevice::ResourceError);
    CHECK(!dev.isOpen());
}

static void testSpi() {
    g_fake = Fake();
    QSpiDevice spi(QStringLiteral("spidev0.1"));
    CHECK(spi.mode() == QSpiDevice::Mode0);
    CHECK(spi.speed() == 1000000);
    CHECK(spi.bitsPerWord() == 8);
    CHECK(spi.bitOrder() == QSpiDevice::MsbFirst);
    CHECK(spi.setMode(QSpiDevice::Mode3));
    CHECK(!spi.setBitsPerWord(0));
    CHECK(!spi.setSpeed(0));
    CHECK(spi.transfer(QByteArray(1, 'x')).isEmpty());
    CHECK(spi.error() == QSpiDevice::NotOpenError);

    CHECK(spi.open());
    CHECK(g_fake.configs.size() == 1);
    CHECK(g_fake.configs.back().mode == 3);
    CHECK(g_fake.configs.back().name == QStringLiteral("spidev0.1"));

    CHECK(spi.setSpeed(8000000));
    CHECK(spi.setBitOrder(QSpiDevice::LsbFirst));
    CHECK(g_fake.configs.size() == 3);
    CHECK(g_fake.configs.back().speed == 8000000);
    CHECK(g_fake.configs.back().lsbFirst);

    const QByteArray in = spi.transfer(QByteArray("\x0f\xf0", 2));
    CHECK(in == QByteArray("\xf0\x0f", 2));
    CHECK(g_fake.duplex);

    CHECK(spi.writeRead(QByteArray(1, '\x9f'), 3) == QByteArray("\x50\x51\x52", 3));
    CHECK(!g_fake.duplex);
    CHECK(g_fake.sent == QByteArray(1, '\x9f'));
    CHECK(g_fake.readSize == 3);

    CHECK(spi.read(2) == QByteArray("\x50\x51", 2));
    CHECK(g_fake.sent.isEmpty());
    CHECK(spi.write(QByteArray("ab")));
    CHECK(g_fake.readSize == 0);
    CHECK(!spi.write(QByteArray()));
    CHECK(spi.error() == QSpiDevice::UnsupportedOperationError);

    // A mode the controller refuses is not kept.
    g_fake.failTransfer = EINVAL;
    CHECK(!spi.setMode(QSpiDevice::Mode1));
    CHECK(spi.mode() == QSpiDevice::Mode3);
    CHECK(spi.isOpen());
    // After a failure, a good transfer reports success again.
    g_fake.failTransfer = 0;
    CHECK(spi.write(QByteArray("c")));
}

// --- The Linux backends ---------------------------------------------------------------------------

static void putFile(const QString &path, const QByteArray &content) {
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

static QByteArray fileText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray("<missing>");
    return file.readAll().trimmed();
}

static void testLinuxPwm(const QString &root) {
    const QString chip = root + QStringLiteral("/class/pwm/pwmchip0");
    putFile(chip + QStringLiteral("/npwm"), "2\n");
    putFile(chip + QStringLiteral("/export"), "");
    putFile(chip + QStringLiteral("/unexport"), "");
    const QString ch = chip + QStringLiteral("/pwm0");
    putFile(ch + QStringLiteral("/period"), "0\n");
    putFile(ch + QStringLiteral("/duty_cycle"), "0\n");
    putFile(ch + QStringLiteral("/enable"), "0\n");
    putFile(ch + QStringLiteral("/polarity"), "normal\n");

    {
        QPwmChannel pwm(QStringLiteral("pwmchip0:0"));
        pwm.setFrequency(1000);
        pwm.setDutyCycle(0.25);
        CHECK(pwm.open());
        CHECK(fileText(ch + QStringLiteral("/period")) == "1000000");
        CHECK(fileText(ch + QStringLiteral("/duty_cycle")) == "250000");
        CHECK(fileText(ch + QStringLiteral("/enable")) == "0");
        CHECK(pwm.setEnabled(true));
        CHECK(fileText(ch + QStringLiteral("/enable")) == "1");
        CHECK(pwm.setPeriod(100000));
        CHECK(fileText(ch + QStringLiteral("/period")) == "100000");
        CHECK(fileText(ch + QStringLiteral("/duty_cycle")) == "25000");
        CHECK(pwm.setPolarity(QPwmChannel::Inversed));
        CHECK(fileText(ch + QStringLiteral("/polarity")) == "inversed");
        CHECK(fileText(ch + QStringLiteral("/enable")) == "1");
    }
    // The channel was exported already, so it is not unexported.
    CHECK(fileText(chip + QStringLiteral("/unexport")).isEmpty());

    {
        QPwmChannel pwm(QStringLiteral("pwmchip0:5"));
        CHECK(!pwm.open());
        CHECK(pwm.error() == QPwmChannel::DeviceNotFoundError);
    }
    {
        QPwmChannel pwm(QStringLiteral("pwmchip7:0"));
        CHECK(!pwm.open());
        CHECK(pwm.error() == QPwmChannel::DeviceNotFoundError);
        pwm.setChannelName(QStringLiteral("pwmchip0"));
        CHECK(!pwm.open());
        CHECK(pwm.error() == QPwmChannel::DeviceNotFoundError);
    }
    {
        // Channel 1 is exported, but no kernel makes its directory here.
        QPwmChannel pwm(QStringLiteral("pwmchip0:1"));
        CHECK(!pwm.open());
        CHECK(pwm.error() == QPwmChannel::TimeoutError);
        CHECK(fileText(chip + QStringLiteral("/export")) == "1");
    }
}

static void testLinuxIio(const QString &root) {
    const QString dev = root + QStringLiteral("/bus/iio/devices/iio:device0");
    putFile(dev + QStringLiteral("/name"), "ads1015\n");
    putFile(dev + QStringLiteral("/in_voltage2_raw"), "1234\n");
    putFile(dev + QStringLiteral("/in_voltage_scale"), "0.5\n");
    putFile(dev + QStringLiteral("/in_voltage3_raw"), "-20\n");
    putFile(dev + QStringLiteral("/in_voltage3_scale"), "2\n");
    putFile(dev + QStringLiteral("/in_voltage3_offset"), "10\n");
    putFile(dev + QStringLiteral("/in_temp_raw"), "25\n");
    putFile(dev + QStringLiteral("/out_voltage0_raw"), "0\n");
    putFile(dev + QStringLiteral("/out_voltage0_scale"), "2\n");

    {
        QAnalogInput adc(QStringLiteral("ads1015:2")); // found by its name
        CHECK(adc.open());
        CHECK(adc.rawValue() == 1234);
        CHECK(adc.hasVoltage());
        CHECK(qFuzzyCompare(adc.voltage(), 0.617));
        putFile(dev + QStringLiteral("/in_voltage2_raw"), "1000\n");
        CHECK(adc.read());
        CHECK(adc.rawValue() == 1000);
    }
    {
        QAnalogInput adc(QStringLiteral("iio:device0:3")); // its own scale and offset
        CHECK(adc.open());
        CHECK(adc.rawValue() == -20);
        CHECK(qFuzzyCompare(adc.voltage() + 1.0, 1.0 - 0.02));
    }
    {
        QAnalogInput adc(QStringLiteral("iio:device0:temp"));
        CHECK(adc.open());
        CHECK(adc.rawValue() == 25);
        CHECK(!adc.hasVoltage());
    }
    {
        QAnalogInput adc(QStringLiteral("iio:device0:7"));
        CHECK(!adc.open());
        CHECK(adc.error() == QAnalogInput::DeviceNotFoundError);
        adc.setChannelName(QStringLiteral("nosuchadc:0"));
        CHECK(!adc.open());
        CHECK(adc.error() == QAnalogInput::DeviceNotFoundError);
        adc.setChannelName(QStringLiteral("iio:device0"));
        CHECK(!adc.open());
        CHECK(adc.error() == QAnalogInput::DeviceNotFoundError);
    }
    {
        QAnalogOutput dac(QStringLiteral("iio:device0:0"));
        dac.setRawValue(5);
        CHECK(dac.open());
        CHECK(fileText(dev + QStringLiteral("/out_voltage0_raw")) == "5");
        CHECK(dac.setVoltage(1.0));
        CHECK(dac.rawValue() == 500);
        CHECK(fileText(dev + QStringLiteral("/out_voltage0_raw")) == "500");
        QAnalogOutput missing(QStringLiteral("iio:device0:1"));
        CHECK(!missing.open());
        CHECK(missing.error() == QAnalogOutput::DeviceNotFoundError);
    }
}

static void testLinuxBuses() {
    {
        QI2cDevice dev(QStringLiteral("nt-no-such-i2c"), 0x20);
        CHECK(!dev.open());
        CHECK(dev.error() == QI2cDevice::DeviceNotFoundError);
        CHECK(dev.errorString().contains(QStringLiteral("/dev/nt-no-such-i2c")));
    }
    {
        QI2cDevice dev(QStringLiteral("/dev/null"), 0x20); // not an I2C adapter
        CHECK(!dev.open());
        CHECK(dev.error() == QI2cDevice::DeviceNotFoundError);
        CHECK(dev.errorString().contains(QStringLiteral("not an I2C bus")));
    }
    {
        QSpiDevice dev(QStringLiteral("nt-no-such-spidev"));
        CHECK(!dev.open());
        CHECK(dev.error() == QSpiDevice::DeviceNotFoundError);
    }
    {
        QSpiDevice dev(QStringLiteral("/dev/null"));
        CHECK(!dev.open());
        CHECK(dev.error() == QSpiDevice::DeviceNotFoundError);
        CHECK(dev.errorString().contains(QStringLiteral("not an SPI device")));
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    useFakes(true);
    testPwm();
    testAnalogInput();
    testAnalogOutput();
    testI2c();
    testSpi();

    useFakes(false);
    QTemporaryDir sys;
    CHECK(sys.isValid());
    QtPrivate::setSysfsRoot(sys.path());
    testLinuxPwm(sys.path());
    testLinuxIio(sys.path());
    QtPrivate::setSysfsRoot(QString());
    testLinuxBuses();

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
