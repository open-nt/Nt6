// MIT License - see LICENSE
//
// The wire protocol between Nt GPIO and a hardware emulator such as ntdisplayemu. When the
// environment variable NT_GPIO_EMULATOR is set, QGpioLine, QSpiDevice and QI2cDevice open a
// connection to the emulator (a Unix domain socket) per object instead of the devices of the
// system, and the emulator plays the displays, switches and LEDs wired to them.
//
// Every message is a frame: a 32-bit little-endian length of what follows, a type byte, and the
// payload. Each request of the client gets one Reply, in order; the emulator also pushes
// GpioLevel frames to the connections of input lines when a switch changes. Levels on the wire
// are physical (high or low): the client applies active low.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

namespace QtPrivate::HwEmulator {

enum MessageType : quint8 {
    // Client to emulator. Each is answered with a Reply.
    OpenGpio = 1,     // u8 direction (0 input, 1 output), u8 level, u8 bias, name; Reply data: u8 level
    OpenSpi = 2,      // u8 mode, u32 speed, u8 bits per word, name
    OpenI2c = 3,      // bus name
    GpioWrite = 4,    // u8 level
    SpiConfigure = 5, // u8 mode, u32 speed, u8 bits per word
    SpiTransfer = 6,  // u8 duplex, u32 read size, data; Reply data: the bytes read
    I2cTransfer = 7,  // u16 address, u32 read size, data; Reply data: the bytes read
    // Emulator to client.
    Reply = 64,     // i32 errno value (0 for success), data
    GpioLevel = 65, // u8 level, pushed to input lines
};

// The bias values of OpenGpio, as QGpioLine::Bias.
enum Bias : quint8 { BiasAsIs = 0, BiasDisabled = 1, BiasPullUp = 2, BiasPullDown = 3 };

constexpr qsizetype HeaderSize = 5;               // length and type
constexpr quint32 MaxFrame = 16u * 1024u * 1024u; // a sanity limit on the length

inline void appendU8(QByteArray &out, quint8 v) { out.append(static_cast<char>(v)); }
inline void appendU16(QByteArray &out, quint16 v) {
    out.append(static_cast<char>(v & 0xff));
    out.append(static_cast<char>(v >> 8));
}
inline void appendU32(QByteArray &out, quint32 v) {
    for (int i = 0; i < 4; ++i)
        out.append(static_cast<char>((v >> (8 * i)) & 0xff));
}
inline quint8 u8At(const QByteArray &in, qsizetype at) { return static_cast<quint8>(in.at(at)); }
inline quint16 u16At(const QByteArray &in, qsizetype at) {
    return static_cast<quint16>(u8At(in, at) | (u8At(in, at + 1) << 8));
}
inline quint32 u32At(const QByteArray &in, qsizetype at) {
    quint32 v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<quint32>(u8At(in, at + i)) << (8 * i);
    return v;
}

// A whole frame of type with payload.
inline QByteArray frame(MessageType type, const QByteArray &payload = QByteArray()) {
    QByteArray out;
    out.reserve(HeaderSize + payload.size());
    appendU32(out, static_cast<quint32>(payload.size() + 1));
    appendU8(out, type);
    out.append(payload);
    return out;
}

// Takes the first whole frame out of buffer: returns false if there is none yet. Sets bad if the
// length is impossible, after which the connection is useless.
inline bool takeFrame(QByteArray &buffer, quint8 &type, QByteArray &payload, bool &bad) {
    bad = false;
    if (buffer.size() < HeaderSize)
        return false;
    const quint32 length = u32At(buffer, 0);
    if (length < 1 || length > MaxFrame) {
        bad = true;
        return false;
    }
    if (buffer.size() < static_cast<qsizetype>(length) + 4)
        return false;
    type = u8At(buffer, 4);
    payload = buffer.mid(HeaderSize, static_cast<qsizetype>(length) - 1);
    buffer.remove(0, static_cast<qsizetype>(length) + 4);
    return true;
}

// The socket path for the value of NT_GPIO_EMULATOR, as QLocalServer resolves a server name: a
// value without a slash is a name in the temporary directory ($TMPDIR, or /tmp).
QString emulatorSocketPath(const QString &value);

} // namespace QtPrivate::HwEmulator
