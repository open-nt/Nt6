// MIT License - see LICENSE
#include "bench.h"

#include "qhwemulatorprotocol_p.h"

#include <QtNetwork/qlocalserver.h>
#include <QtNetwork/qlocalsocket.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>

using namespace QtPrivate::HwEmulator;

// --- SSD1322 -------------------------------------------------------------------------------------

void Ssd1322Model::reset() {
    m_command = -1;
    m_args.clear();
    m_writing = false;
    m_colStart = 0;
    m_colEnd = 119;
    m_rowStart = 0;
    m_rowEnd = 127;
    m_col = m_row = m_byteInGroup = 0;
    m_remap = 0x14;
    m_on = false;
    m_mode = 0xa6;
    m_contrast = 0x7f;
}

void Ssd1322Model::write(const QByteArray &bytes, bool dcHigh) {
    for (const char c : bytes) {
        const auto byte = static_cast<quint8>(c);
        if (!dcHigh) {
            m_writing = false;
            m_command = byte;
            m_args.clear();
            execute();
        } else if (m_writing) {
            writeRam(byte);
        } else if (m_command >= 0) {
            m_args.append(static_cast<char>(byte));
            execute();
        }
    }
}

// Runs the command once it has all its arguments; commands without arguments run at once.
void Ssd1322Model::execute() {
    const auto arg = [this](int i) { return static_cast<int>(static_cast<quint8>(m_args.at(i))); };
    switch (m_command) {
    case 0x15: // column address
        if (m_args.size() == 2) {
            m_colStart = std::min(arg(0), 119);
            m_colEnd = std::min(std::max(arg(1), m_colStart), 119);
            m_col = m_colStart;
            m_byteInGroup = 0;
        }
        break;
    case 0x75: // row address
        if (m_args.size() == 2) {
            m_rowStart = std::min(arg(0), 127);
            m_rowEnd = std::min(std::max(arg(1), m_rowStart), 127);
            m_row = m_rowStart;
        }
        break;
    case 0x5c: // write RAM
        m_writing = true;
        m_col = m_colStart;
        m_row = m_rowStart;
        m_byteInGroup = 0;
        break;
    case 0xa0:
        if (m_args.size() >= 1)
            m_remap = static_cast<quint8>(arg(0));
        break;
    case 0xa4:
    case 0xa5:
    case 0xa6:
    case 0xa7:
        m_mode = m_command;
        break;
    case 0xae:
        m_on = false;
        break;
    case 0xaf:
        m_on = true;
        break;
    case 0xc1:
        if (m_args.size() == 1)
            m_contrast = arg(0);
        break;
    default:
        break; // the analog settings change nothing that can be shown
    }
}

void Ssd1322Model::writeRam(quint8 byte) {
    const int x = m_col * 4 + m_byteInGroup * 2;
    // With nibble remap (bit 2) the high nibble is the left pixel.
    const bool highFirst = (m_remap & 0x04) != 0;
    m_ram[static_cast<std::size_t>(m_row * 480 + x)] = highFirst ? byte >> 4 : byte & 0x0f;
    m_ram[static_cast<std::size_t>(m_row * 480 + x + 1)] = highFirst ? byte & 0x0f : byte >> 4;
    if (++m_byteInGroup < 2)
        return;
    m_byteInGroup = 0;
    if (++m_col <= m_colEnd)
        return;
    m_col = m_colStart;
    if (++m_row > m_rowEnd)
        m_row = m_rowStart;
}

QImage Ssd1322Model::image() const {
    const QSize size = m_wiring.size;
    QImage out(size, QImage::Format_Grayscale8);
    out.fill(0);
    if (!m_on)
        return out;
    // The panel is wired for remap 0x14 (COM scan reversed): without it the picture is upside
    // down, and column remap (bit 1) mirrors it.
    const bool flipV = (m_remap & 0x10) == 0;
    const bool flipH = (m_remap & 0x02) != 0;
    const double scale = 0.35 + 0.65 * m_contrast / 255.0;
    for (int y = 0; y < size.height(); ++y) {
        uchar *line = out.scanLine(y);
        const int ry = flipV ? size.height() - 1 - y : y;
        for (int x = 0; x < size.width(); ++x) {
            const int rx = m_wiring.columnOffset * 4 + (flipH ? size.width() - 1 - x : x);
            int level = rx < 480 ? m_ram[static_cast<std::size_t>(ry * 480 + rx)] : 0;
            if (m_mode == 0xa4)
                level = 0;
            else if (m_mode == 0xa5)
                level = 15;
            else if (m_mode == 0xa7)
                level = 15 - level;
            line[x] = static_cast<uchar>(level * 17 * scale + 0.5);
        }
    }
    return out;
}

// --- HD44780 -------------------------------------------------------------------------------------

void Hd44780Model::nibble(bool rs, quint8 value) {
    value &= 0x0f;
    if (!m_fourBit) {
        // On an 8-bit bus with D0 to D3 not connected, each nibble is a whole instruction.
        if (!rs)
            instruction(static_cast<quint8>(value << 4));
        return;
    }
    if (m_pending < 0) {
        m_pending = value;
        return;
    }
    const auto byte = static_cast<quint8>((m_pending << 4) | value);
    m_pending = -1;
    if (rs)
        data(byte);
    else
        instruction(byte);
}

void Hd44780Model::instruction(quint8 byte) {
    if (byte & 0x80) {
        m_cgramMode = false;
        m_address = byte & 0x7f;
    } else if (byte & 0x40) {
        m_cgramMode = true;
        m_address = byte & 0x3f;
    } else if (byte & 0x20) {
        m_fourBit = (byte & 0x10) == 0;
        m_pending = -1;
        m_twoLines = (byte & 0x08) != 0;
    } else if (byte & 0x10) {
        // Cursor or display shift: only the cursor move is played.
        if (!(byte & 0x08)) {
            m_increment = (byte & 0x04) != 0;
            const bool keep = m_increment;
            advance();
            m_increment = keep;
        }
    } else if (byte & 0x08) {
        m_displayOn = (byte & 0x04) != 0;
        m_cursor = (byte & 0x02) != 0;
        m_blink = (byte & 0x01) != 0;
    } else if (byte & 0x04) {
        m_increment = (byte & 0x02) != 0;
    } else if (byte & 0x02) {
        m_cgramMode = false;
        m_address = 0;
    } else if (byte & 0x01) {
        m_ddram.fill(' ');
        m_cgramMode = false;
        m_address = 0;
        m_increment = true;
    }
}

void Hd44780Model::data(quint8 byte) {
    if (m_cgramMode)
        m_cgram[m_address & 0x3f] = static_cast<char>(byte & 0x1f);
    else
        m_ddram[static_cast<std::size_t>(m_address & 0x7f)] = static_cast<char>(byte);
    advance();
}

// Moves the address counter as the controller does: 0x27 goes on to 0x40 and 0x67 back to 0 on two
// lines, 0x4f back to 0 on one.
void Hd44780Model::advance() {
    if (m_cgramMode) {
        m_address = (m_address + (m_increment ? 1 : -1)) & 0x3f;
        return;
    }
    if (m_twoLines) {
        if (m_increment)
            m_address = m_address == 0x27 ? 0x40 : m_address == 0x67 ? 0x00 : m_address + 1;
        else
            m_address = m_address == 0x40 ? 0x27 : m_address == 0x00 ? 0x67 : m_address - 1;
    } else {
        m_address = m_increment ? (m_address + 1) % 0x50 : (m_address + 0x4f) % 0x50;
    }
}

int Hd44780Model::rowAddress(int row) const {
    const int base = (row & 1) ? 0x40 : 0x00;
    return row >= 2 ? base + m_columns : base;
}

quint8 Hd44780Model::codeAt(int column, int row) const {
    return static_cast<quint8>(m_ddram[static_cast<std::size_t>((rowAddress(row) + column) & 0x7f)]);
}

QPoint Hd44780Model::cursor() const {
    if (m_cgramMode)
        return QPoint(-1, -1);
    for (int r = 0; r < m_rows; ++r) {
        const int start = rowAddress(r);
        if (m_address >= start && m_address < start + m_columns)
            return QPoint(m_address - start, r);
    }
    return QPoint(-1, -1);
}

QString Hd44780Model::text(int row) const {
    QString out;
    for (int c = 0; c < m_columns; ++c)
        out.append(toUnicode(codeAt(c, row)));
    return out;
}

// The reverse of QCharacterDisplay::toDisplayCodes(), for what the A00 ROM shows.
QChar Hd44780Model::toUnicode(quint8 code) {
    struct Mapping {
        quint8 code;
        char16_t to;
    };
    static constexpr Mapping table[] = {
        {0x5c, u'¥'}, {0x7e, u'→'}, {0x7f, u'←'}, {0xdf, u'°'}, {0xe0, u'α'}, {0xe1, u'ä'}, {0xe2, u'β'}, {0xe3, u'ε'},
        {0xe4, u'µ'}, {0xe5, u'σ'}, {0xe6, u'ρ'}, {0xe8, u'√'}, {0xec, u'¢'}, {0xee, u'ñ'}, {0xef, u'ö'}, {0xf2, u'θ'},
        {0xf3, u'∞'}, {0xf4, u'Ω'}, {0xf5, u'ü'}, {0xf6, u'Σ'}, {0xf7, u'π'}, {0xfd, u'÷'}, {0xff, u'█'},
    };
    if (code < 16)
        return QChar(static_cast<char16_t>(code & 7));
    for (const Mapping &m : table) {
        if (m.code == code)
            return QChar(m.to);
    }
    if (code >= 0x20 && code < 0x80)
        return QChar(static_cast<char16_t>(code));
    if (code >= 0xa1 && code <= 0xdf)
        return QChar(static_cast<char16_t>(0xff61 + (code - 0xa1)));
    return QChar(u' ');
}

// --- Bench ---------------------------------------------------------------------------------------

struct Bench::Client {
    QLocalSocket *socket = nullptr;
    QByteArray buffer;
    enum Kind { None, Gpio, Spi, I2c } kind = None;
    QString name;
    bool input = false;
};

Bench::Bench(QObject *parent) : QObject(parent) {}

Bench::~Bench() {
    // Out of the list first: a socket that goes away says so, and dropClient() must not find it.
    std::vector<std::unique_ptr<Client>> clients;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        clients.swap(m_clients);
    }
    for (auto &client : clients)
        delete client->socket;
}

void Bench::addSsd1322(const Ssd1322Model::Wiring &wiring) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_oleds.push_back(std::make_unique<Ssd1322Model>(wiring));
}

void Bench::addHd44780I2c(const QString &bus, int address, int columns, int rows) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CharacterDisplay display;
    display.i2cBus = bus;
    display.i2cAddress = address;
    display.model = std::make_unique<Hd44780Model>(columns, rows);
    m_characters.push_back(std::move(display));
}

void Bench::addHd44780Gpio(const std::array<QString, 6> &lines, int columns, int rows) {
    std::lock_guard<std::mutex> lock(m_mutex);
    CharacterDisplay display;
    display.gpio = lines;
    display.model = std::make_unique<Hd44780Model>(columns, rows);
    m_characters.push_back(std::move(display));
}

void Bench::addSwitch(const QString &name, const QString &label, bool level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Line &l = line(name);
    l.label = label;
    l.level = level;
    l.declared = true;
}

void Bench::addLed(const QString &name, const QString &label) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Line &l = line(name);
    l.label = label;
    l.output = true;
    l.declared = true;
}

void Bench::addButton(const QString &name, const QString &label, bool pressedLevel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Line &l = line(name);
    l.label = label;
    l.level = !pressedLevel;
    l.declared = true;
    l.momentary = true;
    l.pressedLevel = pressedLevel;
}

bool Bench::listen(const QString &name, QString &error) {
    delete m_server;
    m_server = new QLocalServer(this);
    QLocalServer::removeServer(name); // a socket left behind by an emulator that crashed
    if (!m_server->listen(name)) {
        error = m_server->errorString();
        return false;
    }
    QObject::connect(m_server, &QLocalServer::newConnection, this, [this] { accept(); });
    return true;
}

QString Bench::serverPath() const { return m_server ? m_server->fullServerName() : QString(); }

void Bench::accept() {
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        auto client = std::make_unique<Client>();
        client->socket = socket;
        Client *raw = client.get();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_clients.push_back(std::move(client));
        }
        QObject::connect(socket, &QLocalSocket::readyRead, this, [this, raw] { readClient(raw); });
        QObject::connect(socket, &QLocalSocket::disconnected, this, [this, raw] { dropClient(raw); });
        readClient(raw);
    }
}

void Bench::dropClient(Client *client) {
    QLocalSocket *socket = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it =
            std::find_if(m_clients.begin(), m_clients.end(), [client](const auto &c) { return c.get() == client; });
        if (it == m_clients.end())
            return;
        socket = (*it)->socket;
        m_clients.erase(it);
    }
    socket->deleteLater();
    notify();
}

void Bench::readClient(Client *client) {
    client->buffer.append(client->socket->readAll());
    quint8 type = 0;
    QByteArray payload;
    bool bad = false;
    bool any = false;
    while (takeFrame(client->buffer, type, payload, bad)) {
        int code = 0;
        QByteArray data;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            data = handle(client, type, payload, code);
        }
        QByteArray reply;
        appendU32(reply, static_cast<quint32>(code));
        reply.append(data);
        client->socket->write(frame(Reply, reply));
        any = true;
    }
    if (bad) {
        std::fprintf(stderr, "ntdisplayemu: a client sent a malformed frame; dropping it\n");
        client->socket->abort();
        return;
    }
    if (any) {
        client->socket->flush();
        sendPushes();
        notify();
    }
}

Bench::Line &Bench::line(const QString &name) {
    for (Line &l : m_lines) {
        if (l.name == name)
            return l;
    }
    Line l;
    l.name = name;
    m_lines.push_back(l);
    return m_lines.back();
}

// Feeds a change of a line that a program drives to the displays wired to it, and to the programs
// that read it. Under the lock.
void Bench::lineChanged(const QString &name, bool previous, bool level) {
    if (previous != level)
        queuePushes(name, level);
    for (auto &oled : m_oleds) {
        if (name == oled->wiring().reset && previous && !level)
            oled->reset();
    }
    for (CharacterDisplay &display : m_characters) {
        if (display.i2cBus.isEmpty() && name == display.gpio[1] && previous && !level) {
            quint8 value = 0;
            for (std::size_t bit = 0; bit < 4; ++bit) {
                if (line(display.gpio[2 + bit]).level)
                    value = static_cast<quint8>(value | (1u << bit));
            }
            display.model->nibble(line(display.gpio[0]).level, value);
        }
    }
}

QByteArray Bench::handle(Client *client, quint8 type, const QByteArray &payload, int &code) {
    switch (type) {
    case OpenGpio: {
        if (payload.size() < 3 || client->kind != Client::None) {
            code = EINVAL;
            return QByteArray();
        }
        client->kind = Client::Gpio;
        client->name = QString::fromUtf8(payload.mid(3));
        client->input = u8At(payload, 0) == 0;
        const bool known =
            std::any_of(m_lines.begin(), m_lines.end(), [&](const Line &l) { return l.name == client->name; });
        Line &l = line(client->name);
        if (client->input) {
            if (!known)
                l.level = u8At(payload, 2) == BiasPullUp;
        } else {
            const bool previous = l.level;
            // A switch or button declared on the bench stays one: a program that drives its line
            // acts as a second hand on it, and the window can still change it.
            if (!(l.declared && !l.output))
                l.output = true;
            l.level = u8At(payload, 1) != 0;
            lineChanged(l.name, previous, l.level);
        }
        return QByteArray(1, static_cast<char>(line(client->name).level ? 1 : 0));
    }
    case GpioWrite: {
        if (client->kind != Client::Gpio || payload.isEmpty()) {
            code = EINVAL;
            return QByteArray();
        }
        if (client->input) {
            code = EPERM;
            return QByteArray();
        }
        Line &l = line(client->name);
        const bool previous = l.level;
        l.level = u8At(payload, 0) != 0;
        lineChanged(l.name, previous, l.level);
        return QByteArray();
    }
    case OpenSpi: {
        if (payload.size() < 6 || client->kind != Client::None) {
            code = EINVAL;
            return QByteArray();
        }
        const QString name = QString::fromUtf8(payload.mid(6));
        const bool attached =
            std::any_of(m_oleds.begin(), m_oleds.end(), [&](const auto &oled) { return oled->wiring().spi == name; });
        if (!attached) {
            std::fprintf(stderr, "ntdisplayemu: nothing is attached to the SPI device %s\n", name.toUtf8().constData());
            code = ENOENT;
            return QByteArray();
        }
        client->kind = Client::Spi;
        client->name = name;
        return QByteArray();
    }
    case SpiConfigure:
        if (client->kind != Client::Spi)
            code = EINVAL;
        return QByteArray();
    case SpiTransfer: {
        if (client->kind != Client::Spi || payload.size() < 5) {
            code = EINVAL;
            return QByteArray();
        }
        const bool duplex = u8At(payload, 0) != 0;
        const auto readSize = static_cast<qsizetype>(u32At(payload, 1));
        const QByteArray data = payload.mid(5);
        for (auto &oled : m_oleds) {
            if (oled->wiring().spi == client->name) {
                const bool dc = !oled->wiring().dc.isEmpty() && line(oled->wiring().dc).level;
                oled->write(data, dc);
            }
        }
        // The SSD1322 has no output on SPI: what is read is zeros.
        return QByteArray(duplex ? data.size() : std::min<qsizetype>(readSize, MaxFrame / 2), '\0');
    }
    case OpenI2c:
        if (client->kind != Client::None) {
            code = EINVAL;
            return QByteArray();
        }
        client->kind = Client::I2c;
        client->name = QString::fromUtf8(payload);
        return QByteArray();
    case I2cTransfer: {
        if (client->kind != Client::I2c || payload.size() < 6) {
            code = EINVAL;
            return QByteArray();
        }
        const quint16 address = u16At(payload, 0);
        const auto readSize = static_cast<qsizetype>(u32At(payload, 2));
        const QByteArray data = payload.mid(6);
        for (CharacterDisplay &display : m_characters) {
            if (display.i2cBus != client->name || display.i2cAddress != address)
                continue;
            // The PCF8574 latches each byte on its pins; the HD44780 takes a nibble when E falls.
            for (const char c : data) {
                const auto pins = static_cast<quint8>(c);
                if ((display.lastPcf & 0x04) && !(pins & 0x04))
                    display.model->nibble((display.lastPcf & 0x01) != 0, static_cast<quint8>(display.lastPcf >> 4));
                display.lastPcf = pins;
                display.model->setBacklight((pins & 0x08) != 0);
            }
            // Reading a PCF8574 returns its pins.
            return QByteArray(std::min<qsizetype>(readSize, MaxFrame / 2), static_cast<char>(display.lastPcf));
        }
        code = ENXIO; // no device answers at that address
        return QByteArray();
    }
    default:
        code = EINVAL;
        return QByteArray();
    }
}

void Bench::queuePushes(const QString &name, bool level) {
    for (const auto &client : m_clients) {
        if (client->kind == Client::Gpio && client->input && client->name == name)
            m_pushes.emplace_back(client->socket, level);
    }
}

void Bench::sendPushes() {
    std::vector<std::pair<QLocalSocket *, bool>> pushes;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pushes.swap(m_pushes);
    }
    for (const auto &[socket, level] : pushes) {
        QByteArray payload;
        appendU8(payload, level ? 1 : 0);
        socket->write(frame(GpioLevel, payload));
        socket->flush();
    }
}

void Bench::setInput(const QString &name, bool level) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Line &l = line(name);
        if (l.output)
            return; // a program drives it (an LED, or a line nobody declared as an input)
        if (l.level == level)
            return;
        l.level = level;
        queuePushes(name, level);
    }
    sendPushes();
    notify();
}

void Bench::notify() {
    if (changed)
        changed();
}

std::vector<Bench::Line> Bench::lines() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<Line>(m_lines.begin(), m_lines.end());
}

std::vector<QImage> Bench::oledImages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<QImage> out;
    for (const auto &oled : m_oleds)
        out.push_back(oled->image());
    return out;
}

QStringList Bench::characterText(std::size_t display) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QStringList out;
    if (display < m_characters.size()) {
        const Hd44780Model &m = *m_characters[display].model;
        for (int r = 0; r < m.rows(); ++r)
            out.append(m.text(r));
    }
    return out;
}

bool Bench::characterBacklight(std::size_t display) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return display < m_characters.size() && m_characters[display].model->backlight();
}

int Bench::connectionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_clients.size());
}
