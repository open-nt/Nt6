// MIT License - see LICENSE
//
// The ssd1322 platform against fake SPI and GPIO backends: a window painted with QPainter must end
// up in the controller's RAM, which the test rebuilds from the commands on the bus.
#include <QtGpio>
#include <QtGui>

#include "display_fakes.h"
#include "qpa/qplatformintegration_p.h"

#include <cstdio>

using namespace DisplayFakes;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

// The RAM of an SSD1322: 480x128 pixels of 4 bits, written through a column and row window.
struct Ram {
    std::vector<quint8> pixels = std::vector<quint8>(480 * 128, 0);

    void replay(const std::vector<Command> &commands) {
        int c0 = 0, c1 = 119, r0 = 0, r1 = 127;
        for (const Command &cmd : commands) {
            const QByteArray &a = cmd.arguments;
            if (cmd.code == 0x15 && a.size() == 2) {
                c0 = static_cast<quint8>(a.at(0));
                c1 = static_cast<quint8>(a.at(1));
            } else if (cmd.code == 0x75 && a.size() == 2) {
                r0 = static_cast<quint8>(a.at(0));
                r1 = static_cast<quint8>(a.at(1));
            } else if (cmd.code == 0x5c) {
                const int bytesPerRow = (c1 - c0 + 1) * 2;
                for (qsizetype i = 0; i < a.size(); ++i) {
                    const int row = r0 + static_cast<int>(i / bytesPerRow);
                    const int x = c0 * 4 + static_cast<int>(i % bytesPerRow) * 2;
                    if (row > r1)
                        break;
                    const auto b = static_cast<quint8>(a.at(i));
                    pixels[static_cast<std::size_t>(row * 480 + x)] = static_cast<quint8>(b >> 4);
                    pixels[static_cast<std::size_t>(row * 480 + x + 1)] = static_cast<quint8>(b & 0x0f);
                }
            }
        }
    }
    // The gray level of panel pixel (x, y), with the panel at column group 28.
    int at(int x, int y) const { return pixels[static_cast<std::size_t>(y * 480 + 28 * 4 + x)]; }
};

class Window : public QRasterWindow {
public:
    int paints = 0;

protected:
    void paintEvent(QPaintEvent *) override {
        ++paints;
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), Qt::black);
        p.fillRect(QRect(8, 4, 16, 8), Qt::white);
        p.fillRect(QRect(40, 20, 4, 4), QColor(128, 128, 128));
    }
};

} // namespace

static void testOptions() {
    int argc = 0;
    // Not usable without the D/C line, or with an option the platform does not know.
    CHECK(QtPrivate::createPlatformIntegration("ssd1322", argc, nullptr, QString()) == nullptr);
    CHECK(QtPrivate::createPlatformIntegration("ssd1322", argc, nullptr, QStringLiteral("dc=x:foo=1")) == nullptr);
    CHECK(QtPrivate::createPlatformIntegration("ssd1322", argc, nullptr, QStringLiteral("dc=x:size=250x64")) ==
          nullptr);
    CHECK(QtPrivate::createPlatformIntegration("ssd1322", argc, nullptr, QStringLiteral("dc=x:rotate=90")) == nullptr);
    CHECK(QtPrivate::availablePlatforms().contains(QStringLiteral("ssd1322")));
}

int main(int argc, char **argv) {
    install();
    state().dcName = QStringLiteral("gpiochip0:25"); // a name with a colon in the options
    testOptions();
    reset();

    std::vector<char *> args(argv, argv + argc);
    char platform[] = "-platform";
    char spec[] = "ssd1322:spi=spidev2.1:dc=gpiochip0:25:reset=gpiochip0:24:size=128x32:speed=8000000:contrast=0x80";
    args.push_back(platform);
    args.push_back(spec);
    int n = static_cast<int>(args.size());
    args.push_back(nullptr);
    QGuiApplication app(n, args.data());
    CHECK(QGuiApplication::platformName() == QStringLiteral("ssd1322"));
    CHECK(state().spiName == QStringLiteral("spidev2.1"));
    CHECK(state().spiSpeed == 8000000);
    CHECK(state().openedLines.contains(QStringLiteral("gpiochip0:25")));
    CHECK(state().openedLines.contains(QStringLiteral("gpiochip0:24")));
    QScreen *screen = QGuiApplication::primaryScreen();
    CHECK(screen && screen->geometry() == QRect(0, 0, 128, 32));
    CHECK(screen && screen->depth() == 4);

    Window window;
    window.resize(300, 200);
    window.show();
    QElapsedTimer timer;
    timer.start();
    while (window.paints == 0 && timer.elapsed() < 2000)
        QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    CHECK(window.paints > 0);
    CHECK(window.geometry() == QRect(0, 0, 128, 32)); // a top-level window fills the panel

    const std::vector<Command> commands = ssd1322Commands();
    Ram ram;
    ram.replay(commands);
    CHECK(ram.at(0, 0) == 0);
    CHECK(ram.at(8, 4) == 15);
    CHECK(ram.at(23, 11) == 15);
    CHECK(ram.at(24, 11) == 0);
    CHECK(ram.at(7, 4) == 0);
    CHECK(ram.at(41, 21) == 8);
    CHECK(ram.at(127, 31) == 0);
    const Command *contrast = nullptr;
    for (const Command &c : commands) {
        if (c.code == 0xc1)
            contrast = &c;
    }
    CHECK(contrast && contrast->arguments == QByteArray("\x80"));

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
