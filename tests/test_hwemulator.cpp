// MIT License - see LICENSE
//
// Nt GPIO against the ntdisplayemu bench: with NT_GPIO_EMULATOR set, QGpioLine, QSpiDevice and
// QI2cDevice (and the display drivers on top of them) talk to a bench running in a thread of the
// test, and what they do must show on its displays and LEDs; its switches must reach the program.
#include <QtCore>
#include <QtGpio>

#include "bench.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

template <typename F> static bool waitFor(F cond, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    return cond();
}

namespace {

// Runs the bench in a thread of its own, since every call of the program waits for its reply.
class BenchThread : public QThread {
public:
    explicit BenchThread(QString path) : m_path(std::move(path)) {}

    std::atomic<Bench *> bench{nullptr};
    std::atomic<bool> failed{false};

protected:
    void run() override {
        Bench b;
        Ssd1322Model::Wiring wiring;
        wiring.spi = QStringLiteral("spidev0.0");
        wiring.dc = QStringLiteral("gpiochip0:25");
        wiring.reset = QStringLiteral("gpiochip0:24");
        b.addSsd1322(wiring);
        b.addHd44780I2c(QStringLiteral("i2c-1"), 0x27, 20, 4);
        b.addHd44780Gpio({QStringLiteral("rs"), QStringLiteral("e"), QStringLiteral("d4"), QStringLiteral("d5"),
                          QStringLiteral("d6"), QStringLiteral("d7")},
                         16, 2);
        b.addSwitch(QStringLiteral("gpiochip0:17"), QStringLiteral("PTT"));
        b.addButton(QStringLiteral("gpiochip0:30"), QStringLiteral("Select"));
        QString error;
        if (!b.listen(m_path, error)) {
            std::printf("cannot listen: %s\n", error.toUtf8().constData());
            failed = true;
            return;
        }
        bench = &b;
        exec();
        bench = nullptr;
    }

private:
    QString m_path;
};

} // namespace

static void testModels() {
    // The HD44780 model follows the reset sequence from any state, as the controller does.
    Hd44780Model m(16, 2);
    m.nibble(false, 0x3);
    m.nibble(false, 0x3);
    m.nibble(false, 0x3);
    m.nibble(false, 0x2);
    const auto send = [&m](bool rs, int byte) {
        m.nibble(rs, static_cast<quint8>(byte >> 4));
        m.nibble(rs, static_cast<quint8>(byte & 0xf));
    };
    for (const int byte : {0x28, 0x0c, 0x01})
        send(false, byte);
    for (const char c : QByteArray("Hi"))
        send(true, static_cast<quint8>(c));
    CHECK(m.displayOn());
    CHECK(m.text(0) == QStringLiteral("Hi").leftJustified(16));
    CHECK(m.cursor() == QPoint(2, 0));
    CHECK(Hd44780Model::toUnicode(0xdf) == QChar(0x00b0));
    CHECK(Hd44780Model::toUnicode(0xb1) == QChar(0xff71));

    // The SSD1322 model writes RAM through its window and shows nothing while asleep.
    Ssd1322Model::Wiring wiring;
    Ssd1322Model oled(wiring);
    oled.write(QByteArray("\x15", 1), false);
    oled.write(QByteArray("\x1c\x1c", 2), true);
    oled.write(QByteArray("\x75", 1), false);
    oled.write(QByteArray("\x00\x00", 2), true);
    oled.write(QByteArray("\x5c", 1), false);
    oled.write(QByteArray("\xf0\x0a", 2), true);
    CHECK(oled.ramPixel(112, 0) == 15 && oled.ramPixel(115, 0) == 10);
    CHECK(oled.image().pixelColor(0, 0) == QColor(0, 0, 0)); // asleep
    oled.write(QByteArray("\xaf", 1), false);
    CHECK(oled.image().constScanLine(0)[0] > 0); // column group 28 is the left edge of the panel
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testModels();

    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/emu");
    BenchThread thread(path);
    thread.start();
    CHECK(waitFor([&] { return thread.bench.load() != nullptr || thread.failed.load(); }));
    Bench *bench = thread.bench.load();
    if (!bench) {
        std::printf("FAILED: no bench\n");
        return 1;
    }
    qputenv("NT_GPIO_EMULATOR", path.toLocal8Bit());

    // The SSD1322 driver: the frame shows on the emulated panel.
    {
        QSsd1322Display oled;
        oled.setSpiDevice(QStringLiteral("spidev0.0"));
        oled.setDataCommandLine(QStringLiteral("gpiochip0:25"));
        oled.setResetLine(QStringLiteral("gpiochip0:24"));
        CHECK(oled.open());
        QByteArray frame(256 * 64, '\0');
        for (int x = 0; x < 256; ++x)
            frame[10 * 256 + x] = static_cast<char>(0xff); // a white line across row 10
        CHECK(oled.writePixels(reinterpret_cast<const uchar *>(frame.constData()), 256));
        const std::vector<QImage> images = bench->oledImages();
        CHECK(images.size() == 1);
        if (images.size() == 1) {
            CHECK(images[0].size() == QSize(256, 64));
            CHECK(images[0].constScanLine(10)[0] > 100 && images[0].constScanLine(10)[255] > 100);
            CHECK(images[0].constScanLine(11)[128] == 0);
        }
        oled.setDisplayEnabled(false);
        CHECK(bench->oledImages()[0].constScanLine(10)[0] == 0);

        // Nothing is attached to another SPI device.
        QSpiDevice other(QStringLiteral("spidev3.0"));
        CHECK(!other.open());
        CHECK(other.error() == QSpiDevice::DeviceNotFoundError);
    }

    // The HD44780 on I2C, and an address with nothing on it.
    {
        QCharacterDisplay lcd;
        lcd.setSize(20, 4);
        lcd.setI2cDevice(QStringLiteral("i2c-1"), 0x27);
        CHECK(lcd.open());
        CHECK(lcd.setText(QStringLiteral("Temp 21°C\n\nline 3")));
        const QStringList text = bench->characterText(0);
        CHECK(text.size() == 4);
        if (text.size() == 4) {
            CHECK(text[0] == QStringLiteral("Temp 21°C").leftJustified(20));
            CHECK(text[1] == QString(20, QChar(' ')));
            CHECK(text[2] == QStringLiteral("line 3").leftJustified(20));
        }
        CHECK(bench->characterBacklight(0));
        CHECK(lcd.setBacklight(false));
        CHECK(!bench->characterBacklight(0));

        QCharacterDisplay absent;
        absent.setI2cDevice(QStringLiteral("i2c-1"), 0x3f);
        CHECK(!absent.open());
        CHECK(absent.error() == QCharacterDisplay::DeviceError);
    }

    // The HD44780 on GPIO lines.
    {
        QCharacterDisplay lcd;
        lcd.setGpioLines(QStringLiteral("rs"), QStringLiteral("e"), QStringLiteral("d4"), QStringLiteral("d5"),
                         QStringLiteral("d6"), QStringLiteral("d7"));
        CHECK(lcd.open());
        CHECK(lcd.print(QStringLiteral("GPIO bus")));
        const QStringList text = bench->characterText(1);
        CHECK(!text.isEmpty() && text[0].startsWith(QStringLiteral("GPIO bus")));
    }

    // Lines: an output shows as an LED, a switch reaches the program, active low is applied.
    {
        QGpioLine led(QStringLiteral("gpiochip0:5"));
        led.setDirection(QGpioLine::Output);
        CHECK(led.open());
        CHECK(led.setValue(true));
        bool found = false;
        for (const Bench::Line &l : bench->lines()) {
            if (l.name == QStringLiteral("gpiochip0:5")) {
                found = true;
                CHECK(l.output && l.level);
            }
        }
        CHECK(found);

        QGpioLine ptt(QStringLiteral("gpiochip0:17"));
        ptt.setActiveLow(true);
        CHECK(ptt.open());
        CHECK(ptt.isEventDriven());
        CHECK(ptt.value()); // the switch is low, which is active
        std::vector<bool> seen;
        QObject::connect(&ptt, &QGpioLine::valueChanged, [&seen](bool v) { seen.push_back(v); });
        QMetaObject::invokeMethod(
            bench, [bench] { bench->setInput(QStringLiteral("gpiochip0:17"), true); }, Qt::QueuedConnection);
        CHECK(waitFor([&] { return !seen.empty(); }));
        CHECK(!seen.empty() && !seen.back());
        CHECK(!ptt.value());

        // A pull-up input that the bench did not know starts high.
        QGpioLine button(QStringLiteral("gpiochip1:3"));
        button.setBias(QGpioLine::PullUp);
        CHECK(button.open());
        CHECK(button.value());
        CHECK(waitFor([&] { return bench->connectionCount() >= 3; }));
    }

    // A push button rests high (released) and is low while held; a line one program drives is read
    // by the others.
    {
        QGpioLine select(QStringLiteral("gpiochip0:30"));
        select.setActiveLow(true);
        CHECK(select.open());
        CHECK(!select.value());
        std::vector<bool> presses;
        QObject::connect(&select, &QGpioLine::valueChanged, [&presses](bool v) { presses.push_back(v); });
        QMetaObject::invokeMethod(
            bench, [bench] { bench->setInput(QStringLiteral("gpiochip0:30"), false); }, Qt::QueuedConnection);
        CHECK(waitFor([&] { return !presses.empty(); }));
        CHECK(!presses.empty() && presses.back());

        QGpioLine reader(QStringLiteral("wire"));
        CHECK(reader.open());
        std::vector<bool> levels;
        QObject::connect(&reader, &QGpioLine::valueChanged, [&levels](bool v) { levels.push_back(v); });
        QGpioLine driver(QStringLiteral("wire"));
        driver.setDirection(QGpioLine::Output);
        CHECK(driver.open());
        CHECK(driver.setValue(true));
        CHECK(waitFor([&] { return !levels.empty(); }));
        CHECK(!levels.empty() && levels.back());
    }

    // When the emulator goes away, calls fail instead of hanging.
    {
        QGpioLine out(QStringLiteral("gpiochip0:6"));
        out.setDirection(QGpioLine::Output);
        CHECK(out.open());
        thread.quit();
        thread.wait();
        CHECK(!out.setValue(true));
        QGpioLine late(QStringLiteral("gpiochip0:7"));
        CHECK(!late.open());
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
