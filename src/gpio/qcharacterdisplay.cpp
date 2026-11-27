// MIT License - see LICENSE
#include <QtGpio/qcharacterdisplay.h>
#include <QtGpio/qgpioline.h>
#include <QtGpio/qi2cdevice.h>

#include <QtCore/qthread.h>

#include <array>

namespace {

// HD44780 instructions.
constexpr quint8 CmdClear = 0x01;
constexpr quint8 CmdHome = 0x02;
constexpr quint8 CmdEntryMode = 0x04;      // | 0x02 increment
constexpr quint8 CmdDisplayControl = 0x08; // | 0x04 display, 0x02 cursor, 0x01 blink
constexpr quint8 CmdFunctionSet = 0x20;    // | 0x08 two lines (4-bit bus, 5x8 dots)
constexpr quint8 CmdSetCgram = 0x40;
constexpr quint8 CmdSetDdram = 0x80;

// The waits of the datasheet, rounded up: most instructions take 37 us, clear and home 1.52 ms.
constexpr unsigned long ShortWaitUs = 50;
constexpr unsigned long LongWaitUs = 2000;

// The bits of a PCF8574 backpack as nearly all of them are wired: P0 RS, P1 RW, P2 E, P3 the
// backlight transistor, P4 to P7 the data lines D4 to D7.
constexpr quint8 PcfRs = 0x01;
constexpr quint8 PcfEnable = 0x04;
constexpr quint8 PcfBacklight = 0x08;

// How the controller is reached. Every byte goes as two nibbles, high nibble first.
class LcdBus {
public:
    LcdBus() = default;
    virtual ~LcdBus() = default;
    LcdBus(const LcdBus &) = delete;
    LcdBus &operator=(const LcdBus &) = delete;

    virtual bool open(QString &error) = 0;
    // Sends bytes to the instruction register (rs false) or to the data register, each as two
    // nibbles, waiting the short instruction time after each byte.
    virtual bool send(const QByteArray &bytes, bool rs, QString &error) = 0;
    // Sends one nibble to the instruction register, for the reset sequence.
    virtual bool sendNibble(quint8 nibble, QString &error) = 0;
    virtual bool setBacklight(bool on, QString &error) = 0;
};

class I2cBus final : public LcdBus {
public:
    I2cBus(const QString &busName, int address) : m_device(busName, address) {}

    bool open(QString &error) override {
        if (!m_device.open())
            return fail(error);
        return write(QByteArray(1, static_cast<char>(m_backlight)), error);
    }

    bool send(const QByteArray &bytes, bool rs, QString &error) override {
        // One transaction for all of it: at 100 or 400 kHz each byte on the bus takes longer than
        // an instruction needs, so the controller keeps up without waits of its own.
        QByteArray out;
        out.reserve(bytes.size() * 6);
        for (const char c : bytes) {
            const auto byte = static_cast<quint8>(c);
            appendNibble(out, static_cast<quint8>(byte >> 4), rs);
            appendNibble(out, static_cast<quint8>(byte & 0x0f), rs);
        }
        return write(out, error);
    }

    bool sendNibble(quint8 nibble, QString &error) override {
        QByteArray out;
        appendNibble(out, nibble, false);
        return write(out, error);
    }

    bool setBacklight(bool on, QString &error) override {
        m_backlight = on ? PcfBacklight : 0;
        return write(QByteArray(1, static_cast<char>(m_backlight)), error);
    }

private:
    // The data first, then E high and low again with the data held.
    void appendNibble(QByteArray &out, quint8 nibble, bool rs) const {
        const auto bits = static_cast<quint8>((nibble << 4) | (rs ? PcfRs : 0) | m_backlight);
        out.append(static_cast<char>(bits));
        out.append(static_cast<char>(bits | PcfEnable));
        out.append(static_cast<char>(bits));
    }

    bool write(const QByteArray &data, QString &error) {
        if (m_device.write(data))
            return true;
        return fail(error);
    }

    bool fail(QString &error) {
        error = m_device.errorString();
        return false;
    }

    QI2cDevice m_device;
    quint8 m_backlight = PcfBacklight;
};

class GpioBus final : public LcdBus {
public:
    GpioBus(const std::array<QString, 6> &names, const QString &backlight) {
        for (std::size_t i = 0; i < names.size(); ++i)
            m_lines[i].setLineName(names[i]);
        if (!backlight.isEmpty()) {
            m_backlight = std::make_unique<QGpioLine>();
            m_backlight->setLineName(backlight);
        }
    }

    bool open(QString &error) override {
        for (QGpioLine &line : m_lines) {
            line.setDirection(QGpioLine::Output);
            if (!line.open()) {
                error = line.errorString();
                return false;
            }
        }
        if (m_backlight) {
            m_backlight->setDirection(QGpioLine::Output);
            if (!m_backlight->open() || !m_backlight->setValue(true)) {
                error = m_backlight->errorString();
                return false;
            }
        }
        return true;
    }

    bool send(const QByteArray &bytes, bool rs, QString &error) override {
        if (!set(m_lines[Rs], rs, error))
            return false;
        for (const char c : bytes) {
            const auto byte = static_cast<quint8>(c);
            if (!pulse(static_cast<quint8>(byte >> 4), error) || !pulse(static_cast<quint8>(byte & 0x0f), error))
                return false;
            QThread::usleep(ShortWaitUs);
        }
        return true;
    }

    bool sendNibble(quint8 nibble, QString &error) override {
        return set(m_lines[Rs], false, error) && pulse(nibble, error);
    }

    bool setBacklight(bool on, QString &error) override {
        if (!m_backlight) {
            error = QStringLiteral("No backlight line was set");
            return false;
        }
        return set(*m_backlight, on, error);
    }

private:
    // Indexes into m_lines.
    static constexpr std::size_t Rs = 0, Enable = 1, D4 = 2;

    static bool set(QGpioLine &line, bool value, QString &error) {
        if (line.setValue(value))
            return true;
        error = line.errorString();
        return false;
    }

    // Puts nibble on D4 to D7 and latches it on the falling edge of E. Each line change is a
    // system call or a driver call, which takes longer than the 450 ns E must stay high.
    bool pulse(quint8 nibble, QString &error) {
        for (std::size_t bit = 0; bit < 4; ++bit) {
            if (!set(m_lines[D4 + bit], ((nibble >> bit) & 1) != 0, error))
                return false;
        }
        if (!set(m_lines[Enable], true, error))
            return false;
        QThread::usleep(1);
        return set(m_lines[Enable], false, error);
    }

    std::array<QGpioLine, 6> m_lines;
    std::unique_ptr<QGpioLine> m_backlight;
};

} // namespace

class QCharacterDisplayPrivate {
public:
    explicit QCharacterDisplayPrivate(QCharacterDisplay *owner) : q(owner) {}

    void setError(QCharacterDisplay::DisplayError e, const QString &message) {
        error = e;
        errorString = message;
        if (e != QCharacterDisplay::NoError)
            emit q->errorOccurred(e);
    }

    bool ready() {
        if (bus)
            return true;
        setError(QCharacterDisplay::NotOpenError, QStringLiteral("The display is not open"));
        return false;
    }

    bool command(quint8 instruction, unsigned long waitUs = 0) {
        if (!ready())
            return false;
        QString message;
        if (!bus->send(QByteArray(1, static_cast<char>(instruction)), false, message)) {
            setError(QCharacterDisplay::DeviceError, message);
            return false;
        }
        if (waitUs)
            QThread::usleep(waitUs);
        return true;
    }

    bool data(const QByteArray &bytes) {
        if (!ready())
            return false;
        if (bytes.isEmpty())
            return true;
        QString message;
        if (!bus->send(bytes, true, message)) {
            setError(QCharacterDisplay::DeviceError, message);
            return false;
        }
        return true;
    }

    bool applyControl() {
        return command(static_cast<quint8>(CmdDisplayControl | (displayOn ? 0x04 : 0) | (cursorOn ? 0x02 : 0) |
                                           (blinkOn ? 0x01 : 0)));
    }

    // The DDRAM address of the start of row r: rows 2 and 3 continue rows 0 and 1 in memory.
    int rowAddress(int r) const {
        const int base = (r & 1) ? 0x40 : 0x00;
        return r >= 2 ? base + columns : base;
    }

    bool initialize() {
        // The reset sequence of the datasheet for a 4-bit bus, which works whatever state the
        // controller is in: three times "8-bit", then "4-bit".
        QThread::msleep(50);
        QString message;
        struct Step {
            quint8 nibble;
            unsigned long waitUs;
        };
        static constexpr Step steps[] = {{0x3, 4500}, {0x3, 150}, {0x3, 150}, {0x2, ShortWaitUs}};
        for (const Step &step : steps) {
            if (!bus->sendNibble(step.nibble, message)) {
                setError(QCharacterDisplay::DeviceError, message);
                return false;
            }
            QThread::usleep(step.waitUs);
        }
        return command(static_cast<quint8>(CmdFunctionSet | (rows > 1 ? 0x08 : 0))) && command(CmdDisplayControl) &&
               command(CmdClear, LongWaitUs) && command(CmdEntryMode | 0x02) && applyControl();
    }

    QCharacterDisplay *q;
    QCharacterDisplay::Bus busType = QCharacterDisplay::I2cPcf8574;
    int columns = 16;
    int rows = 2;
    QString i2cBus;
    int i2cAddress = 0x27;
    std::array<QString, 6> gpioNames;
    QString backlightName;
    std::unique_ptr<LcdBus> bus;
    int row = 0;
    bool displayOn = true;
    bool cursorOn = false;
    bool blinkOn = false;
    bool backlight = true;
    QCharacterDisplay::DisplayError error = QCharacterDisplay::NoError;
    QString errorString;
};

/*!
    \class QCharacterDisplay
    \inmodule NtGpio
    \brief The QCharacterDisplay class drives a character LCD with a Hitachi HD44780 controller.
    \since 6.0

    The HD44780 and the many controllers compatible with it (Sitronix ST7066, Samsung KS0066,
    Winstar WS0010 and others) drive text displays of 8 to 40 columns and 1 to 4 rows. A
    QCharacterDisplay reaches one in either of two ways:

    \list
    \li \c I2cPcf8574, the default: through the PCF8574 I2C backpack soldered onto most modules
        sold today. Its address is usually \c 0x27, or \c 0x3f for the PCF8574A.
    \li \c Gpio4Bit: through six GPIO lines (RS, E and D4 to D7), with R/W tied to ground, and an
        optional seventh line that switches the backlight.
    \endlist

    \code
    QCharacterDisplay lcd;
    lcd.setSize(20, 4);
    lcd.setI2cDevice(QStringLiteral("i2c-1"), 0x27);
    if (!lcd.open())
        qFatal("%s", qPrintable(lcd.errorString()));
    lcd.setText(QStringLiteral("Nt on an HD44780\nTemp 21.5°C"));
    \endcode

    Text is translated to the character ROM the controller is most often made with (A00, with
    ASCII, Japanese katakana and a few Greek letters and symbols); see toDisplayCodes(). Codes 0
    to 7 show the custom characters set with setCustomCharacter(), and writeData() sends codes as
    they are, for displays with another ROM.

    The bus classes it uses, QI2cDevice and QGpioLine, work on Linux and on Zephyr, so the class
    does too. Each call blocks until the controller has the instructions: about 0.5 ms per
    character over I2C at 100 kHz, and 2 ms more for clear() and home().

    \differences
    Qt has no such class; QCharacterDisplay is an Nt extension. There is no 8-bit bus, no reading
    back (R/W must be tied low), no SPI shift-register backpack and no 5x10 font.
    \enddifferences

    \sa QSsd1322Display, QI2cDevice, QGpioLine
*/

/*!
    \enum QCharacterDisplay::Bus

    This enum describes how the display is connected.

    \value I2cPcf8574 Through a PCF8574 or PCF8574A I2C port expander, wired as on the common
           backpacks: P0 RS, P1 R/W, P2 E, P3 backlight, P4 to P7 D4 to D7.
    \value Gpio4Bit Through six GPIO lines, with a 4-bit data bus.
*/

/*!
    \enum QCharacterDisplay::DisplayError

    This enum describes the errors of a display.

    \value NoError No error occurred.
    \value DeviceError The bus or a line failed; errorString() says which and why.
    \value NotOpenError The operation needs an open display.
    \value UnsupportedOperationError The settings or arguments are not possible.
*/

/*!
    Constructs a display object with the given \a parent. Choose the bus and open() it.
*/
QCharacterDisplay::QCharacterDisplay(QObject *parent)
    : QObject(parent), d(std::make_unique<QCharacterDisplayPrivate>(this)) {}

/*!
    Destroys the display object. The display keeps showing what it showed.
*/
QCharacterDisplay::~QCharacterDisplay() = default;

/*!
    Sets the way the display is connected to \a bus. It takes effect at the next open().

    \sa setI2cDevice(), setGpioLines()
*/
void QCharacterDisplay::setBus(Bus bus) { d->busType = bus; }

/*!
    Returns how the display is connected.
*/
QCharacterDisplay::Bus QCharacterDisplay::bus() const { return d->busType; }

/*!
    Sets the size of the display to \a columns characters by \a rows rows. Values out of the
    range the controller can address (1 to 40 columns, 1 to 4 rows, at most 80 characters) are
    ignored. It takes effect at the next open().
*/
void QCharacterDisplay::setSize(int columns, int rows) {
    if (columns < 1 || columns > 40 || rows < 1 || rows > 4 || columns * rows > 80)
        return;
    d->columns = columns;
    d->rows = rows;
}

/*!
    Returns the number of characters in a row.
*/
int QCharacterDisplay::columns() const { return d->columns; }

/*!
    Returns the number of rows.
*/
int QCharacterDisplay::rows() const { return d->rows; }

/*!
    Sets the I2C \a bus (as QI2cDevice names it, such as \c i2c-1) and the \a address of the
    PCF8574 backpack, and selects the \c I2cPcf8574 bus.
*/
void QCharacterDisplay::setI2cDevice(const QString &bus, int address) {
    d->busType = I2cPcf8574;
    d->i2cBus = bus;
    d->i2cAddress = address;
}

/*!
    Sets the names of the GPIO lines, as QGpioLine names them, wired to the RS (\a
    registerSelect), E (\a enable) and data pins of the controller (\a d4, \a d5, \a d6 and
    \a d7), and selects the \c Gpio4Bit bus.

    \sa setBacklightLine()
*/
void QCharacterDisplay::setGpioLines(const QString &registerSelect, const QString &enable, const QString &d4,
                                     const QString &d5, const QString &d6, const QString &d7) {
    d->busType = Gpio4Bit;
    d->gpioNames = {registerSelect, enable, d4, d5, d6, d7};
}

/*!
    Sets the name of the GPIO \a line that switches the backlight on a \c Gpio4Bit bus; high is
    on. Without one, setBacklight() fails.
*/
void QCharacterDisplay::setBacklightLine(const QString &line) { d->backlightName = line; }

/*!
    Opens the bus, resets the controller, clears the display and switches it and the backlight
    on. Returns \c true on success; otherwise sets error() and returns \c false. An open display is
    closed first.
*/
bool QCharacterDisplay::open() {
    close();
    clearError();
    std::unique_ptr<LcdBus> bus;
    if (d->busType == I2cPcf8574) {
        if (d->i2cBus.isEmpty()) {
            d->setError(UnsupportedOperationError, QStringLiteral("No I2C bus was set"));
            return false;
        }
        bus = std::make_unique<I2cBus>(d->i2cBus, d->i2cAddress);
    } else {
        for (const QString &name : d->gpioNames) {
            if (name.isEmpty()) {
                d->setError(UnsupportedOperationError, QStringLiteral("Not all six GPIO lines were set"));
                return false;
            }
        }
        bus = std::make_unique<GpioBus>(d->gpioNames, d->backlightName);
    }
    QString message;
    if (!bus->open(message)) {
        d->setError(DeviceError, message);
        return false;
    }
    d->bus = std::move(bus);
    d->row = 0;
    d->displayOn = true;
    d->cursorOn = false;
    d->blinkOn = false;
    d->backlight = true;
    if (!d->initialize()) {
        d->bus.reset();
        return false;
    }
    return true;
}

/*!
    Releases the bus. The display keeps showing what it showed.
*/
void QCharacterDisplay::close() { d->bus.reset(); }

/*!
    Returns \c true if the display is open.
*/
bool QCharacterDisplay::isOpen() const { return d->bus != nullptr; }

/*!
    Clears the display and moves the cursor to the top left. Returns \c true on success.
*/
bool QCharacterDisplay::clear() {
    d->row = 0;
    return d->command(CmdClear, LongWaitUs);
}

/*!
    Moves the cursor to the top left. Returns \c true on success.
*/
bool QCharacterDisplay::home() {
    d->row = 0;
    return d->command(CmdHome, LongWaitUs);
}

/*!
    Moves the cursor to \a column in \a row, both counted from 0. Returns \c true on success, and
    \c false if the position is outside the display.
*/
bool QCharacterDisplay::setCursorPosition(int column, int row) {
    if (column < 0 || column >= d->columns || row < 0 || row >= d->rows) {
        d->setError(UnsupportedOperationError, QStringLiteral("The position is outside the display"));
        return false;
    }
    if (!d->command(static_cast<quint8>(CmdSetDdram | (d->rowAddress(row) + column))))
        return false;
    d->row = row;
    return true;
}

/*!
    Writes \a text at the cursor, translated with toDisplayCodes(). A newline moves to the start of
    the next row, and lines below the last row are dropped; text past the end of a row is not
    wrapped. Returns \c true on success.

    \sa setText()
*/
bool QCharacterDisplay::print(const QString &text) {
    const QStringList lines = text.split(QChar('\n'));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            if (d->row + 1 >= d->rows)
                return true; // lines below the last row are dropped
            if (!setCursorPosition(0, d->row + 1))
                return false;
        }
        if (!d->data(toDisplayCodes(lines.at(i))))
            return false;
    }
    return true;
}

/*!
    Replaces what the display shows with \a text, one line per row, each padded with spaces or
    cut to the width of the display. Rows without a line are blanked. Unlike clear() followed by
    print(), the display does not flicker. Returns \c true on success.
*/
bool QCharacterDisplay::setText(const QString &text) {
    const QStringList lines = text.split(QChar('\n'));
    for (int r = 0; r < d->rows; ++r) {
        QByteArray codes = r < lines.size() ? toDisplayCodes(lines.at(r)) : QByteArray();
        codes = codes.left(d->columns);
        codes.append(QByteArray(d->columns - codes.size(), ' '));
        if (!setCursorPosition(0, r) || !d->data(codes))
            return false;
    }
    return true;
}

/*!
    Writes \a codes at the cursor as they are, as character codes of the display's ROM. Returns
    \c true on success.
*/
bool QCharacterDisplay::writeData(const QByteArray &codes) { return d->data(codes); }

/*!
    Sets custom character \a slot (0 to 7) to \a pattern: eight bytes, one per row from the top,
    with the five pixels of a row in the low bits (bit 4 is the leftmost). The character shows
    wherever code \a slot is written, as \c{QChar(slot)} in text. The cursor moves to the top
    left. Returns \c true on success.
*/
bool QCharacterDisplay::setCustomCharacter(int slot, const QByteArray &pattern) {
    if (slot < 0 || slot > 7 || pattern.size() != 8) {
        d->setError(UnsupportedOperationError, QStringLiteral("A custom character is a slot 0 to 7 and 8 rows"));
        return false;
    }
    QByteArray rows = pattern;
    for (char &row : rows)
        row = static_cast<char>(row & 0x1f);
    return d->command(static_cast<quint8>(CmdSetCgram | (slot << 3))) && d->data(rows) && setCursorPosition(0, 0);
}

/*!
    Switches the display on if \a enabled is \c true, or off; the text is kept either way. Returns
    \c true on success.
*/
bool QCharacterDisplay::setDisplayEnabled(bool enabled) {
    d->displayOn = enabled;
    return d->applyControl();
}

/*!
    Shows an underline cursor if \a visible is \c true. Returns \c true on success.
*/
bool QCharacterDisplay::setCursorVisible(bool visible) {
    d->cursorOn = visible;
    return d->applyControl();
}

/*!
    Makes the character at the cursor blink if \a blinking is \c true. Returns \c true on success.
*/
bool QCharacterDisplay::setCursorBlinking(bool blinking) {
    d->blinkOn = blinking;
    return d->applyControl();
}

/*!
    Switches the backlight on if \a on is \c true, or off. Returns \c true on success.
*/
bool QCharacterDisplay::setBacklight(bool on) {
    if (!d->ready())
        return false;
    QString message;
    if (!d->bus->setBacklight(on, message)) {
        d->setError(d->busType == Gpio4Bit && d->backlightName.isEmpty() ? UnsupportedOperationError : DeviceError,
                    message);
        return false;
    }
    d->backlight = on;
    return true;
}

/*!
    Returns \c true if the backlight is on.
*/
bool QCharacterDisplay::hasBacklight() const { return d->backlight; }

/*!
    Returns \a text as character codes of the A00 ROM of the HD44780. Codes 0 to 7 (the custom
    characters) and printable ASCII stay as they are, except backslash and tilde, which the ROM
    lacks. The yen sign, the arrows \c{U+2190} and \c{U+2192}, the half-width katakana, the degree
    sign, and the letters and symbols α ä β ß ε µ σ ρ √ ¢ ñ ö θ ∞ Ω ü Σ π ÷ and the full block
    \c{U+2588} map to their places in the ROM. Anything else becomes \c{?}.
*/
QByteArray QCharacterDisplay::toDisplayCodes(const QString &text) {
    struct Mapping {
        char16_t from;
        quint8 to;
    };
    static constexpr Mapping table[] = {
        {u'¢', 0xec}, {u'¥', 0x5c}, {u'°', 0xdf}, {u'µ', 0xe4}, {u'ß', 0xe2}, {u'ä', 0xe1}, {u'ñ', 0xee},
        {u'ö', 0xef}, {u'÷', 0xfd}, {u'ü', 0xf5}, {u'Σ', 0xf6}, {u'Ω', 0xf4}, {u'α', 0xe0}, {u'β', 0xe2},
        {u'ε', 0xe3}, {u'θ', 0xf2}, {u'μ', 0xe4}, {u'π', 0xf7}, {u'ρ', 0xe6}, {u'σ', 0xe5}, {u'←', 0x7f},
        {u'→', 0x7e}, {u'√', 0xe8}, {u'∞', 0xf3}, {u'█', 0xff},
    };
    QByteArray out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char16_t c = text.at(i).unicode();
        quint8 code = '?';
        if (c < 8 || (c >= 0x20 && c < 0x7e && c != u'\\')) {
            code = static_cast<quint8>(c);
        } else if (c >= 0xff61 && c <= 0xff9f) {
            code = static_cast<quint8>(0xa1 + (c - 0xff61));
        } else if (c >= 0xd800 && c < 0xdc00) {
            if (i + 1 < text.size() && text.at(i + 1).unicode() >= 0xdc00 && text.at(i + 1).unicode() < 0xe000)
                ++i; // one '?' for the whole surrogate pair
        } else {
            for (const Mapping &m : table) {
                if (m.from == c) {
                    code = m.to;
                    break;
                }
            }
        }
        out.append(static_cast<char>(code));
    }
    return out;
}

/*!
    Returns the last error that occurred.
*/
QCharacterDisplay::DisplayError QCharacterDisplay::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QCharacterDisplay::errorString() const { return d->errorString; }

/*!
    Resets error() to \c NoError.
*/
void QCharacterDisplay::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qcharacterdisplay.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
