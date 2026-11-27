// MIT License - see LICENSE
//
// The workbench of ntdisplayemu: the displays, switches and LEDs it plays, and the server that Nt
// GPIO connects to when NT_GPIO_EMULATOR is set (see src/gpio/qhwemulatorprotocol_p.h). The
// window in main.cpp only draws what the bench holds; the tests drive the bench directly.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qimage.h>

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class QLocalServer;
class QLocalSocket;

// An SSD1322 controller: 480x128 pixels of RAM with 4 bits each, of which the panel shows a window.
class Ssd1322Model {
public:
    struct Wiring {
        QString spi = QStringLiteral("spidev0.0");
        QString dc;
        QString reset;
        QSize size{256, 64};
        int columnOffset = 28;
    };

    explicit Ssd1322Model(const Wiring &wiring) : m_wiring(wiring) { reset(); }

    const Wiring &wiring() const { return m_wiring; }
    // What came over SPI with D/C at dcHigh.
    void write(const QByteArray &bytes, bool dcHigh);
    // The controller after a pulse on RES: asleep, RAM kept.
    void reset();
    // The panel as the eye sees it: Format_Grayscale8, black while the controller sleeps.
    QImage image() const;
    bool isOn() const { return m_on; }
    // The 4-bit level of RAM pixel (x, y), for tests.
    int ramPixel(int x, int y) const { return m_ram[static_cast<std::size_t>(y * 480 + x)]; }

private:
    void execute();
    void writeRam(quint8 byte);

    Wiring m_wiring;
    std::vector<quint8> m_ram = std::vector<quint8>(480 * 128, 0);
    int m_command = -1;
    QByteArray m_args;
    bool m_writing = false;
    int m_colStart = 0, m_colEnd = 119, m_rowStart = 0, m_rowEnd = 127;
    int m_col = 0, m_row = 0, m_byteInGroup = 0;
    quint8 m_remap = 0x14;
    bool m_on = false;
    int m_mode = 0xa6; // a4 all off, a5 all on, a6 normal, a7 inverse
    int m_contrast = 0x7f;
};

// An HD44780 controller on a 4-bit bus, with the A00 character ROM.
class Hd44780Model {
public:
    Hd44780Model(int columns, int rows) : m_columns(columns), m_rows(rows) { m_ddram.fill(' '); }

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    // One nibble latched on the falling edge of E, with RS at rs.
    void nibble(bool rs, quint8 value);
    void setBacklight(bool on) { m_backlight = on; }
    bool backlight() const { return m_backlight; }
    bool displayOn() const { return m_displayOn; }
    bool cursorVisible() const { return m_cursor; }
    bool cursorBlinking() const { return m_blink; }
    // The character code shown at (column, row).
    quint8 codeAt(int column, int row) const;
    // The 8 rows of custom character code (0 to 15, mirrored), 5 bits each.
    QByteArray glyph(quint8 code) const { return m_cgram.mid((code & 7) * 8, 8); }
    // The text of row as Unicode; custom characters are QChar(0) to QChar(7).
    QString text(int row) const;
    // The cursor as (column, row), or (-1, -1) when it is off the visible part.
    QPoint cursor() const;
    static QChar toUnicode(quint8 code);

private:
    void instruction(quint8 byte);
    void data(quint8 byte);
    int rowAddress(int row) const;
    void advance();

    int m_columns;
    int m_rows;
    std::array<char, 128> m_ddram{};
    QByteArray m_cgram = QByteArray(64, '\0');
    int m_address = 0;
    bool m_cgramMode = false;
    bool m_increment = true;
    bool m_fourBit = false;
    int m_pending = -1; // the high nibble waiting for its low one
    bool m_twoLines = true;
    bool m_displayOn = false;
    bool m_cursor = false;
    bool m_blink = false;
    bool m_backlight = true;
};

class Bench : public QObject {
public:
    struct Line {
        QString name;
        QString label;
        bool output = false; // opened as an output by a program, or declared an LED
        bool level = false;  // physical
        bool declared = false;
        bool momentary = false; // a push button: at pressedLevel only while it is held
        bool pressedLevel = false;
    };
    struct CharacterDisplay {
        QString i2cBus; // with i2cAddress, or empty for a GPIO bus
        int i2cAddress = 0x27;
        std::array<QString, 6> gpio; // rs, e, d4 .. d7
        std::unique_ptr<Hd44780Model> model;
        quint8 lastPcf = 0;
    };

    explicit Bench(QObject *parent = nullptr);
    ~Bench() override;

    void addSsd1322(const Ssd1322Model::Wiring &wiring);
    void addHd44780I2c(const QString &bus, int address, int columns, int rows);
    void addHd44780Gpio(const std::array<QString, 6> &lines, int columns, int rows);
    void addSwitch(const QString &name, const QString &label, bool level = false);
    void addLed(const QString &name, const QString &label);
    // A push button that pulls the line to pressedLevel while it is held (low: a button to ground
    // with a pull-up, the usual wiring), and leaves it at the other level otherwise.
    void addButton(const QString &name, const QString &label, bool pressedLevel = false);

    // Listens on name (see QLocalServer::listen()). Returns false and sets error if it cannot.
    bool listen(const QString &name, QString &error);
    QString serverPath() const;

    // Sets an input line, as a switch would, and tells the programs that have it open.
    void setInput(const QString &name, bool level);

    // Snapshots, safe to take from any thread.
    std::vector<Line> lines() const;
    std::vector<QImage> oledImages() const;
    QStringList characterText(std::size_t display) const;
    bool characterBacklight(std::size_t display) const;
    int connectionCount() const;

    // For the window, in the thread of the bench; under the lock while it runs.
    template <typename F> void read(F f) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        f(m_oleds, m_characters, m_lines);
    }

    // Called in the thread of the bench after anything changed.
    std::function<void()> changed;

private:
    struct Client;
    void accept();
    void readClient(Client *client);
    void dropClient(Client *client);
    QByteArray handle(Client *client, quint8 type, const QByteArray &payload, int &code);
    Line &line(const QString &name);
    void lineChanged(const QString &name, bool previous, bool level);
    void queuePushes(const QString &name, bool level);
    void sendPushes();
    void notify();

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<Ssd1322Model>> m_oleds;
    std::vector<CharacterDisplay> m_characters;
    std::deque<Line> m_lines; // a deque, so that line() keeps references to the others valid
    std::vector<std::unique_ptr<Client>> m_clients;
    // Levels to push to input lines once the lock is released: a line that one program drives is
    // read by the others, as a wire between two boards would be.
    std::vector<std::pair<QLocalSocket *, bool>> m_pushes;
    QLocalServer *m_server = nullptr;
};
