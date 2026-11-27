// MIT License - see LICENSE
//
// QCharacterDisplay and QSsd1322Display against fake I2C, SPI and GPIO backends: the bytes that
// reach the bus are decoded back into HD44780 instructions and SSD1322 commands.
#include <QtCore>
#include <QtGpio>

#include "display_fakes.h"

#include <cerrno>
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

using Bytes = std::vector<std::pair<bool, quint8>>;

static Bytes instructions(std::initializer_list<int> codes) {
    Bytes out;
    for (const int c : codes)
        out.emplace_back(false, static_cast<quint8>(c));
    return out;
}

static Bytes characters(const QByteArray &text) {
    Bytes out;
    for (const char c : text)
        out.emplace_back(true, static_cast<quint8>(c));
    return out;
}

static Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes &p : parts)
        out.insert(out.end(), p.begin(), p.end());
    return out;
}

static const std::vector<std::pair<bool, quint8>> kResetNibbles = {{false, 3}, {false, 3}, {false, 3}, {false, 2}};

static void testCodes() {
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("Hi 42!")) == QByteArray("Hi 42!"));
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("a\\b~c")) == QByteArray("a?b?c"));
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("21°C")) == QByteArray("21\xdf"
                                                                                  "C"));
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("¥→←π█")) == QByteArray("\x5c\x7e\x7f\xf7\xff"));
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("ｱﾝ")) == QByteArray("\xb1\xdd")); // katakana
    CHECK(QCharacterDisplay::toDisplayCodes(QString(QChar(3))) == QByteArray(1, '\x03'));     // custom char
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("x\U0001F600y")) == QByteArray("x?y"));
    CHECK(QCharacterDisplay::toDisplayCodes(QStringLiteral("é\t")) == QByteArray("??"));
}

static void testCharacterI2c() {
    reset();
    QCharacterDisplay lcd;
    CHECK(lcd.bus() == QCharacterDisplay::I2cPcf8574);
    CHECK(lcd.columns() == 16 && lcd.rows() == 2);
    CHECK(!lcd.open()); // no bus
    CHECK(lcd.error() == QCharacterDisplay::UnsupportedOperationError);
    CHECK(!lcd.clear());

    lcd.setSize(20, 4);
    lcd.setSize(41, 2); // ignored
    CHECK(lcd.columns() == 20 && lcd.rows() == 4);
    lcd.setI2cDevice(QStringLiteral("i2c-1"), 0x3f);
    CHECK(lcd.open());
    CHECK(lcd.isOpen());
    CHECK(lcd.error() == QCharacterDisplay::NoError);
    CHECK(state().i2cBus == QStringLiteral("i2c-1"));
    CHECK(state().i2cAddress == 0x3f);
    CHECK(!state().i2c.isEmpty() && (static_cast<quint8>(state().i2c.at(0)) & 0x08)); // backlight on
    Bytes nibbles = pcf8574Nibbles();
    CHECK(nibbles.size() >= 4);
    CHECK(Bytes(nibbles.begin(), nibbles.begin() + 4) == kResetNibbles);
    CHECK(joinNibbles(nibbles, 4) == instructions({0x28, 0x08, 0x01, 0x06, 0x0c}));

    // setText(): every row, padded to the width; rows 2 and 3 start at 0x14 and 0x54.
    state().i2c.clear();
    CHECK(lcd.setText(QStringLiteral("Hello\nNt")));
    const QByteArray blank(20, ' ');
    CHECK(joinNibbles(pcf8574Nibbles(), 0) ==
          concat({instructions({0x80}), characters(QByteArray("Hello").leftJustified(20)), instructions({0xc0}),
                  characters(QByteArray("Nt").leftJustified(20)), instructions({0x94}), characters(blank),
                  instructions({0xd4}), characters(blank)}));

    // print(): newline moves to the next row; lines past the last row are dropped.
    CHECK(lcd.setCursorPosition(3, 2));
    state().i2c.clear();
    CHECK(lcd.print(QStringLiteral("ab\ncd\nef")));
    CHECK(joinNibbles(pcf8574Nibbles(), 0) == concat({characters("ab"), instructions({0xd4}), characters("cd")}));
    CHECK(!lcd.setCursorPosition(20, 0));
    CHECK(lcd.error() == QCharacterDisplay::UnsupportedOperationError);

    // Display control and backlight.
    state().i2c.clear();
    CHECK(lcd.setCursorVisible(true));
    CHECK(lcd.setCursorBlinking(true));
    CHECK(lcd.setDisplayEnabled(false));
    CHECK(joinNibbles(pcf8574Nibbles(), 0) == instructions({0x0e, 0x0f, 0x0b}));
    state().i2c.clear();
    CHECK(lcd.setBacklight(false));
    CHECK(!lcd.hasBacklight());
    CHECK(state().i2c == QByteArray(1, '\0'));
    state().i2c.clear();
    CHECK(lcd.writeData(QByteArray("\x01", 1)));
    CHECK(!(static_cast<quint8>(state().i2c.at(0)) & 0x08)); // the backlight stays off

    // A custom character goes to CGRAM, then the cursor goes home.
    state().i2c.clear();
    CHECK(lcd.setCustomCharacter(2, QByteArray("\x1f\x11\x11\x11\x11\x11\x1f\xff", 8)));
    CHECK(joinNibbles(pcf8574Nibbles(), 0) ==
          concat({instructions({0x50}), characters(QByteArray("\x1f\x11\x11\x11\x11\x11\x1f\x1f", 8)),
                  instructions({0x80})}));
    CHECK(!lcd.setCustomCharacter(8, QByteArray(8, '\0')));

    lcd.close();
    CHECK(!lcd.isOpen());
    CHECK(!lcd.print(QStringLiteral("x")));
    CHECK(lcd.error() == QCharacterDisplay::NotOpenError);
}

static void testCharacterGpio() {
    reset();
    state().enableName = QStringLiteral("e");
    QCharacterDisplay lcd;
    lcd.setSize(16, 1);
    lcd.setGpioLines(QStringLiteral("rs"), QStringLiteral("e"), QStringLiteral("d4"), QStringLiteral("d5"),
                     QStringLiteral("d6"), QStringLiteral("d7"));
    CHECK(lcd.bus() == QCharacterDisplay::Gpio4Bit);
    CHECK(lcd.open());
    CHECK(state().openedLines.size() == 6);
    const Bytes nibbles = state().gpioNibbles;
    CHECK(nibbles.size() >= 4 && Bytes(nibbles.begin(), nibbles.begin() + 4) == kResetNibbles);
    CHECK(joinNibbles(nibbles, 4) == instructions({0x20, 0x08, 0x01, 0x06, 0x0c})); // one line
    state().gpioNibbles.clear();
    CHECK(lcd.print(QStringLiteral("Ok")));
    CHECK(joinNibbles(state().gpioNibbles, 0) == characters("Ok"));
    CHECK(!lcd.setBacklight(true)); // no backlight line
    CHECK(lcd.error() == QCharacterDisplay::UnsupportedOperationError);

    reset();
    state().enableName = QStringLiteral("e");
    lcd.setBacklightLine(QStringLiteral("bl"));
    CHECK(lcd.open());
    CHECK(state().levels.value(QStringLiteral("bl")));
    CHECK(lcd.setBacklight(false));
    CHECK(!state().levels.value(QStringLiteral("bl")));

    reset();
    state().failGpioOpen = EBUSY;
    CHECK(!lcd.open());
    CHECK(lcd.error() == QCharacterDisplay::DeviceError);
    CHECK(!lcd.isOpen());
    state().failGpioOpen = 0;

    QCharacterDisplay partial;
    partial.setGpioLines(QStringLiteral("rs"), QString(), QStringLiteral("d4"), QStringLiteral("d5"),
                         QStringLiteral("d6"), QStringLiteral("d7"));
    CHECK(!partial.open());
    CHECK(partial.error() == QCharacterDisplay::UnsupportedOperationError);
}

static const Command *find(const std::vector<Command> &commands, quint8 code, std::size_t from = 0) {
    for (std::size_t i = from; i < commands.size(); ++i) {
        if (commands[i].code == code)
            return &commands[i];
    }
    return nullptr;
}

static void testSsd1322() {
    reset();
    state().dcName = QStringLiteral("dc");
    QSsd1322Display oled;
    CHECK(oled.width() == 256 && oled.height() == 64);
    CHECK(oled.speed() == 10000000);
    CHECK(oled.columnOffset() == 28);
    CHECK(!oled.open()); // no SPI device or D/C line
    CHECK(oled.error() == QSsd1322Display::UnsupportedOperationError);

    oled.setSpiDevice(QStringLiteral("spidev1.0"));
    oled.setDataCommandLine(QStringLiteral("dc"));
    oled.setResetLine(QStringLiteral("res"));
    oled.setSize(250, 64); // not a multiple of 4: ignored
    CHECK(oled.width() == 256);
    CHECK(oled.open());
    CHECK(state().spiName == QStringLiteral("spidev1.0"));
    CHECK(state().spiSpeed == 10000000);
    CHECK(state().levels.value(QStringLiteral("res"))); // out of reset
    CHECK(state().largestSpiTransfer <= 4096);

    std::vector<Command> commands = ssd1322Commands();
    CHECK(!commands.empty() && commands.front().code == 0xfd && commands.front().arguments == QByteArray("\x12"));
    const Command *mux = find(commands, 0xca);
    CHECK(mux && mux->arguments == QByteArray(1, 63));
    const Command *remap = find(commands, 0xa0);
    CHECK(remap && remap->arguments == QByteArray("\x14\x11"));
    const Command *contrast = find(commands, 0xc1);
    CHECK(contrast && contrast->arguments == QByteArray("\x9f"));
    // The panel is cleared before it is switched on: 28..91 is 256 pixels.
    const Command *columns = find(commands, 0x15);
    CHECK(columns && columns->arguments == QByteArray("\x1c\x5b"));
    const Command *ram = find(commands, 0x5c);
    CHECK(ram && ram->arguments == QByteArray(8192, '\0'));
    CHECK(commands.back().code == 0xaf);

    // A region is widened to groups of four pixels and packed two pixels per byte.
    QByteArray frame(256 * 64, '\0');
    auto *pixels = reinterpret_cast<uchar *>(frame.data());
    pixels[10 * 256 + 4] = 0xff;
    pixels[10 * 256 + 5] = 0x80;
    pixels[11 * 256 + 7] = 0x1f;
    state().spi.clear();
    CHECK(oled.writePixels(pixels, 256, QRect(5, 10, 3, 2)));
    commands = ssd1322Commands();
    CHECK(commands.size() == 3);
    if (commands.size() == 3) {
        CHECK(commands[0].code == 0x15 && commands[0].arguments == QByteArray("\x1d\x1d"));
        CHECK(commands[1].code == 0x75 && commands[1].arguments == QByteArray("\x0a\x0b"));
        CHECK(commands[2].code == 0x5c && commands[2].arguments == QByteArray("\xf8\x00\x00\x01", 4));
    }
    // Areas outside the panel send nothing.
    state().spi.clear();
    CHECK(oled.writePixels(pixels, 256, QRect(300, 0, 10, 10)));
    CHECK(state().spi.empty());
    CHECK(!oled.writePixels(pixels, 100));
    CHECK(oled.error() == QSsd1322Display::UnsupportedOperationError);

    state().spi.clear();
    CHECK(oled.setContrast(0x40));
    CHECK(!oled.setContrast(300));
    CHECK(oled.contrast() == 0x40);
    CHECK(oled.setInverted(true));
    CHECK(oled.setDisplayEnabled(false));
    oled.setRotated(true);
    CHECK(oled.writeCommand(0xb1, QByteArray("\x74")));
    commands = ssd1322Commands();
    CHECK(commands.size() == 5);
    if (commands.size() == 5) {
        CHECK(commands[0].code == 0xc1 && commands[0].arguments == QByteArray("\x40"));
        CHECK(commands[1].code == 0xa7);
        CHECK(commands[2].code == 0xae);
        CHECK(commands[3].code == 0xa0 && commands[3].arguments == QByteArray("\x06\x11"));
        CHECK(commands[4].code == 0xb1 && commands[4].arguments == QByteArray("\x74"));
    }

    // A failing bus is reported.
    int errors = 0;
    QObject::connect(&oled, &QSsd1322Display::errorOccurred, [&errors](QSsd1322Display::DisplayError) { ++errors; });
    state().failSpi = EIO;
    CHECK(!oled.clear());
    CHECK(oled.error() == QSsd1322Display::DeviceError);
    CHECK(!oled.errorString().isEmpty());
    CHECK(errors == 1);
    state().failSpi = 0;

    oled.close();
    CHECK(!oled.isOpen());
    CHECK(!oled.clear());
    CHECK(oled.error() == QSsd1322Display::NotOpenError);

    // Opening fails cleanly when the bus does, and a panel that does not fit the RAM is refused.
    state().failSpi = EIO;
    CHECK(!oled.open());
    CHECK(!oled.isOpen());
    CHECK(oled.error() == QSsd1322Display::DeviceError);
    state().failSpi = 0;
    oled.setColumnOffset(70);
    CHECK(!oled.open());
    CHECK(oled.error() == QSsd1322Display::UnsupportedOperationError);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    install();
    testCodes();
    testCharacterI2c();
    testCharacterGpio();
    testSsd1322();
    std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
