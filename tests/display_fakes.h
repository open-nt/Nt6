// MIT License - see LICENSE
//
// Fake SPI, I2C and GPIO backends for the display tests: they record what the drivers send, and
// decode it back into HD44780 instructions and SSD1322 commands.
#pragma once

#include "qgpiobackend_p.h"
#include "qperipheral_p.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace DisplayFakes {

struct State {
    QHash<QString, bool> levels; // the GPIO lines by name
    QString dcName;              // the line that is D/C of the SSD1322
    QString enableName;          // the line that is E of the HD44780 on GPIO
    QStringList openedLines;
    QString spiName;
    quint32 spiSpeed = 0;
    QString i2cBus;
    quint16 i2cAddress = 0;
    qsizetype largestSpiTransfer = 0;
    int failSpi = 0; // an errno value SPI transfers fail with, or 0
    int failGpioOpen = 0;
    // What reached the devices.
    struct SpiChunk {
        bool dc;
        QByteArray data;
    };
    std::vector<SpiChunk> spi;
    QByteArray i2c; // every byte written to the PCF8574, in order
    // HD44780 on GPIO: (rs, nibble) at each falling edge of E.
    std::vector<std::pair<bool, quint8>> gpioNibbles;
};

inline State &state() {
    static State s;
    return s;
}

inline void reset() {
    const QString dc = state().dcName;
    const QString e = state().enableName;
    state() = State();
    state().dcName = dc;
    state().enableName = e;
}

class Gpio final : public QtPrivate::GpioBackend {
public:
    bool open(const QtPrivate::GpioConfig &config, QtPrivate::GpioFailure &failure) override {
        if (state().failGpioOpen) {
            QtPrivate::setGpioFailure(failure, state().failGpioOpen, QStringLiteral("Cannot request the line"));
            return false;
        }
        m_name = config.name;
        state().openedLines.append(m_name);
        state().levels[m_name] = config.initialValue;
        return true;
    }
    int eventDescriptor() const override { return -1; }
    bool readEvents(std::vector<bool> &levels, QtPrivate::GpioFailure &) override {
        levels.clear();
        return true;
    }
    bool readValue(bool &value, QtPrivate::GpioFailure &) override {
        value = state().levels.value(m_name);
        return true;
    }
    bool writeValue(bool value, QtPrivate::GpioFailure &) override {
        State &s = state();
        const bool was = s.levels.value(m_name);
        s.levels[m_name] = value;
        if (m_name == s.enableName && was && !value) {
            // D4 to D7 are the lines called d4 .. d7, RS the one called rs.
            quint8 nibble = 0;
            for (int bit = 0; bit < 4; ++bit) {
                if (s.levels.value(QStringLiteral("d%1").arg(4 + bit)))
                    nibble = static_cast<quint8>(nibble | (1 << bit));
            }
            s.gpioNibbles.emplace_back(s.levels.value(QStringLiteral("rs")), nibble);
        }
        return true;
    }

private:
    QString m_name;
};

class Spi final : public QtPrivate::SpiBackend {
public:
    bool open(const QtPrivate::SpiConfig &config, QtPrivate::HwFailure &) override {
        state().spiName = config.name;
        state().spiSpeed = config.speed;
        return true;
    }
    bool configure(const QtPrivate::SpiConfig &config, QtPrivate::HwFailure &) override {
        state().spiSpeed = config.speed;
        return true;
    }
    bool transfer(const QByteArray &data, qsizetype, bool, QByteArray &read, QtPrivate::HwFailure &failure) override {
        State &s = state();
        if (s.failSpi) {
            QtPrivate::setHwFailure(failure, s.failSpi, QStringLiteral("Fake SPI failure"), false);
            return false;
        }
        s.largestSpiTransfer = std::max(s.largestSpiTransfer, data.size());
        s.spi.push_back({s.levels.value(s.dcName), data});
        read.clear();
        return true;
    }
};

class I2c final : public QtPrivate::I2cBackend {
public:
    bool open(const QString &bus, QtPrivate::HwFailure &) override {
        state().i2cBus = bus;
        return true;
    }
    bool transfer(quint16 address, const QByteArray &data, qsizetype, QByteArray &read,
                  QtPrivate::HwFailure &) override {
        state().i2cAddress = address;
        state().i2c.append(data);
        read.clear();
        return true;
    }
};

inline std::unique_ptr<QtPrivate::GpioBackend> makeGpio() { return std::make_unique<Gpio>(); }
inline std::unique_ptr<QtPrivate::SpiBackend> makeSpi() { return std::make_unique<Spi>(); }
inline std::unique_ptr<QtPrivate::I2cBackend> makeI2c() { return std::make_unique<I2c>(); }

inline void install() {
    QtPrivate::setGpioBackendFactory(makeGpio);
    QtPrivate::setSpiBackendFactory(makeSpi);
    QtPrivate::setI2cBackendFactory(makeI2c);
}

// An SSD1322 command and its arguments, as decoded from the SPI traffic.
struct Command {
    quint8 code;
    QByteArray arguments;
};

inline std::vector<Command> ssd1322Commands() {
    std::vector<Command> out;
    for (const State::SpiChunk &chunk : state().spi) {
        if (!chunk.dc) {
            for (const char c : chunk.data)
                out.push_back({static_cast<quint8>(c), QByteArray()});
        } else if (!out.empty()) {
            out.back().arguments.append(chunk.data);
        }
    }
    return out;
}

// HD44780 traffic as (rs, nibble) pairs, from the PCF8574 bytes: a nibble is latched when E falls.
inline std::vector<std::pair<bool, quint8>> pcf8574Nibbles() {
    std::vector<std::pair<bool, quint8>> out;
    const QByteArray &bytes = state().i2c;
    for (qsizetype i = 1; i < bytes.size(); ++i) {
        const auto prev = static_cast<quint8>(bytes.at(i - 1));
        const auto cur = static_cast<quint8>(bytes.at(i));
        if ((prev & 0x04) && !(cur & 0x04))
            out.emplace_back((prev & 0x01) != 0, static_cast<quint8>(prev >> 4));
    }
    return out;
}

// Joins nibbles into (rs, byte) pairs, after skipping `skip` single nibbles of the reset sequence.
inline std::vector<std::pair<bool, quint8>> joinNibbles(const std::vector<std::pair<bool, quint8>> &nibbles,
                                                        std::size_t skip) {
    std::vector<std::pair<bool, quint8>> out;
    for (std::size_t i = skip; i + 1 < nibbles.size(); i += 2)
        out.emplace_back(nibbles[i].first, static_cast<quint8>((nibbles[i].second << 4) | nibbles[i + 1].second));
    return out;
}

} // namespace DisplayFakes
