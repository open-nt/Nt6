// MIT License - see LICENSE
//
// Parsers of the register logs of OPL music: VGM (and gzipped VGZ), DOSBox Raw OPL (DRO, versions
// 1 and 2) and id Software Music Format (IMF, types 0 and 1). Each becomes a list of register
// writes timed in seconds. Input is untrusted: every read is bounds-checked, and the number of
// events is capped.

#include "qfmmusic_p.h"

#include <QtCore/qendian.h>

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif

#include <algorithm>
#include <limits>

namespace QtFM::Private {

namespace {

constexpr std::size_t kMaxEvents = 16u * 1024u * 1024u;

class Reader {
public:
    explicit Reader(const QByteArray &data)
        : m_data(reinterpret_cast<const quint8 *>(data.constData())), m_size(static_cast<std::size_t>(data.size())) {}
    bool has(std::size_t pos, std::size_t n) const { return pos <= m_size && n <= m_size - pos; }
    quint8 u8(std::size_t pos) const { return has(pos, 1) ? m_data[pos] : 0; }
    quint16 u16(std::size_t pos) const { return has(pos, 2) ? qFromLittleEndian<quint16>(m_data + pos) : 0; }
    quint32 u32(std::size_t pos) const { return has(pos, 4) ? qFromLittleEndian<quint32>(m_data + pos) : 0; }
    std::size_t size() const { return m_size; }

private:
    const quint8 *m_data;
    std::size_t m_size;
};

bool fail(QString *error, const char *message) {
    if (error)
        *error = QString::fromLatin1(message);
    return false;
}

bool parseVgm(const Reader &r, MusicData &m, QString *error) {
    const quint32 version = r.u32(0x08);
    const quint32 opl2Clock = version >= 0x151 ? r.u32(0x50) : 0;
    const quint32 opl1Clock = version >= 0x151 ? r.u32(0x54) : 0;
    const quint32 opl3Clock = version >= 0x151 ? r.u32(0x5c) : 0;
    if (!opl2Clock && !opl1Clock && !opl3Clock)
        return fail(error, "the VGM file has no OPL2 or OPL3 part");
    m.chip = opl3Clock ? ChipType::OPL3 : ChipType::OPL2;
    std::size_t pos = version >= 0x150 && r.u32(0x34) ? 0x34 + r.u32(0x34) : 0x40;
    const std::size_t loopPos = r.u32(0x1c) ? 0x1c + std::size_t(r.u32(0x1c)) : 0;
    quint64 samples = 0;
    auto time = [&samples] { return double(samples) / 44100.0; };
    while (r.has(pos, 1) && m.events.size() < kMaxEvents) {
        if (loopPos && pos == loopPos) {
            m.loopTime = time();
            m.loopIndex = m.events.size();
        }
        const quint8 cmd = r.u8(pos);
        if (cmd == 0x66)
            break;
        switch (cmd) {
        case 0x5a: // YM3812
        case 0x5b: // YM3526
            m.events.push_back({time(), r.u8(pos + 1), r.u8(pos + 2)});
            pos += 3;
            continue;
        case 0x5e: // YMF262 port 0
        case 0x5f: // YMF262 port 1
            m.events.push_back(
                {time(), static_cast<quint16>((cmd == 0x5f ? 0x100 : 0) | r.u8(pos + 1)), r.u8(pos + 2)});
            pos += 3;
            continue;
        case 0x61:
            samples += r.u16(pos + 1);
            pos += 3;
            continue;
        case 0x62:
            samples += 735;
            pos += 1;
            continue;
        case 0x63:
            samples += 882;
            pos += 1;
            continue;
        case 0x67: { // data block: 0x67 0x66 type size32
            const quint32 size = r.u32(pos + 3);
            if (!r.has(pos + 7, size))
                return fail(error, "truncated VGM data block");
            pos += 7 + std::size_t(size);
            continue;
        }
        case 0xe0:
            pos += 5;
            continue;
        default:
            break;
        }
        if (cmd >= 0x70 && cmd <= 0x7f) {
            samples += (cmd & 0x0f) + 1u;
            pos += 1;
        } else if (cmd >= 0x80 && cmd <= 0x8f) {
            samples += cmd & 0x0fu;
            pos += 1;
        } else if ((cmd >= 0x30 && cmd <= 0x3f) || cmd == 0x4f || cmd == 0x50) {
            pos += 2;
        } else if ((cmd >= 0x40 && cmd <= 0x4e) || (cmd >= 0x51 && cmd <= 0x5f) || (cmd >= 0xa0 && cmd <= 0xbf)) {
            pos += 3;
        } else if (cmd >= 0xc0 && cmd <= 0xdf) {
            pos += 4;
        } else if (cmd >= 0xe1) {
            pos += 5;
        } else {
            return fail(error, "unknown VGM command");
        }
    }
    m.duration = time();
    return true;
}

bool parseDro(const Reader &r, MusicData &m, QString *error) {
    const quint16 major = r.u16(8);
    const quint16 minor = r.u16(10);
    double ms = 0;
    if (major == 2 && minor == 0) {
        const quint32 pairs = r.u32(12);
        const quint8 hardware = r.u8(20);
        const quint8 format = r.u8(21);
        const quint8 compression = r.u8(22);
        const quint8 shortDelay = r.u8(23);
        const quint8 longDelay = r.u8(24);
        const quint8 codemapLength = r.u8(25);
        if (format != 0 || compression != 0 || codemapLength > 128)
            return fail(error, "unsupported DRO format");
        m.chip = hardware == 0 ? ChipType::OPL2 : ChipType::OPL3;
        std::size_t pos = 26 + std::size_t(codemapLength);
        for (quint32 i = 0; i < pairs && r.has(pos, 2) && m.events.size() < kMaxEvents; ++i, pos += 2) {
            const quint8 code = r.u8(pos);
            const quint8 value = r.u8(pos + 1);
            if (code == shortDelay) {
                ms += value + 1;
            } else if (code == longDelay) {
                ms += (value + 1) * 256.0;
            } else {
                const quint8 index = code & 0x7f;
                if (index >= codemapLength)
                    continue;
                const auto reg = static_cast<quint16>(((code & 0x80) ? 0x100 : 0) | r.u8(26 + std::size_t(index)));
                m.events.push_back({ms / 1000.0, reg, value});
            }
        }
    } else {
        // Version 0.1: the version is a 32-bit 0x00010000, then the length in ms and bytes, then the
        // hardware type in one byte (some writers pad it to four).
        const quint32 bytes = r.u32(16);
        const quint8 hardware = r.u8(20);
        m.chip = hardware == 0 ? ChipType::OPL2 : ChipType::OPL3;
        std::size_t pos =
            (r.u8(21) == 0 && r.u8(22) == 0 && r.u8(23) == 0 && r.size() >= 24 + std::size_t(bytes)) ? 24 : 21;
        const std::size_t end = std::min(r.size(), pos + std::size_t(bytes));
        int bank = 0;
        while (pos < end && m.events.size() < kMaxEvents) {
            const quint8 code = r.u8(pos++);
            switch (code) {
            case 0x00:
                ms += r.u8(pos++) + 1;
                break;
            case 0x01:
                ms += r.u16(pos) + 1;
                pos += 2;
                break;
            case 0x02:
            case 0x03:
                bank = code - 2;
                break;
            case 0x04:
                m.events.push_back({ms / 1000.0, static_cast<quint16>((bank << 8) | r.u8(pos)), r.u8(pos + 1)});
                pos += 2;
                break;
            default:
                m.events.push_back({ms / 1000.0, static_cast<quint16>((bank << 8) | code), r.u8(pos)});
                pos += 1;
                break;
            }
        }
    }
    m.duration = ms / 1000.0;
    return true;
}

bool parseImf(const Reader &r, MusicData &m, int rate, QString *error) {
    if (r.size() < 4)
        return fail(error, "the IMF file is too short");
    // Type 1 starts with the length of the data; type 0 starts with the data.
    std::size_t pos = 0;
    std::size_t end = r.size();
    const quint16 length = r.u16(0);
    if (length && length % 4 == 0 && std::size_t(length) + 2 <= r.size()) {
        pos = 2;
        end = 2 + std::size_t(length);
    }
    m.chip = ChipType::OPL2;
    quint64 ticks = 0;
    for (; pos + 4 <= end && m.events.size() < kMaxEvents; pos += 4) {
        m.events.push_back({double(ticks) / rate, r.u8(pos), r.u8(pos + 1)});
        ticks += r.u16(pos + 2);
    }
    m.duration = double(ticks) / rate;
    return true;
}

} // namespace

QByteArray gunzip(const QByteArray &data) {
#ifdef NT_HAVE_ZLIB
    z_stream stream{};
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        return QByteArray();
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = static_cast<uInt>(std::min<qsizetype>(data.size(), std::numeric_limits<uInt>::max()));
    QByteArray out;
    int status = Z_OK;
    constexpr qsizetype chunk = 64 * 1024;
    constexpr qsizetype limit = 256 * 1024 * 1024;
    while (status == Z_OK && out.size() < limit) {
        const qsizetype used = out.size();
        out.resize(used + chunk);
        stream.next_out = reinterpret_cast<Bytef *>(out.data() + used);
        stream.avail_out = static_cast<uInt>(chunk);
        status = inflate(&stream, Z_NO_FLUSH);
        out.resize(used + chunk - static_cast<qsizetype>(stream.avail_out));
    }
    inflateEnd(&stream);
    return status == Z_STREAM_END ? out : QByteArray();
#else
    Q_UNUSED(data);
    return QByteArray();
#endif
}

std::unique_ptr<MusicData> parseMusic(const QByteArray &input, MusicFormat format, int imfRate, QString *error) {
    QByteArray data = input;
    if (data.startsWith("\x1f\x8b")) {
        data = gunzip(input);
        if (data.isEmpty()) {
            fail(error, "cannot inflate the gzip data");
            return nullptr;
        }
    }
    if (format == MusicFormat::Auto) {
        if (data.startsWith("Vgm "))
            format = MusicFormat::Vgm;
        else if (data.startsWith("DBRAWOPL"))
            format = MusicFormat::Dro;
        else
            format = MusicFormat::Imf;
    }
    auto music = std::make_unique<MusicData>();
    const Reader reader(data);
    bool ok = false;
    switch (format) {
    case MusicFormat::Vgm:
        ok = data.startsWith("Vgm ") ? parseVgm(reader, *music, error) : fail(error, "not a VGM file");
        break;
    case MusicFormat::Dro:
        ok = data.startsWith("DBRAWOPL") ? parseDro(reader, *music, error) : fail(error, "not a DRO file");
        break;
    case MusicFormat::Imf:
        ok = parseImf(reader, *music, std::clamp(imfRate, 1, 10000), error);
        break;
    case MusicFormat::Auto:
        break;
    }
    if (!ok)
        return nullptr;
    if (music->events.empty()) {
        fail(error, "the music has no register writes");
        return nullptr;
    }
    music->core = ChipCore::create(music->chip);
    if (music->loopTime < 0 || music->loopIndex > music->events.size())
        music->loopTime = -1;
    return music;
}

} // namespace QtFM::Private
