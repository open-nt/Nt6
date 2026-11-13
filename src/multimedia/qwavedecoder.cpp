// MIT License - see LICENSE
#include "qwavedecoder_p.h"

#include <QtCore/qendian.h>

#include <algorithm>
#include <cstring>

namespace QtMultimediaPrivate {

namespace {

constexpr quint16 kFormatPcm = 1;
constexpr quint16 kFormatFloat = 3;
constexpr quint16 kFormatExtensible = 0xFFFE;

bool readExactly(QIODevice *device, char *data, qint64 size) {
    qint64 done = 0;
    while (done < size) {
        const qint64 n = device->read(data + done, size - done);
        if (n <= 0)
            return false;
        done += n;
    }
    return true;
}

bool skip(QIODevice *device, qint64 size) {
    char buffer[4096];
    while (size > 0) {
        const qint64 n = device->read(buffer, std::min<qint64>(size, qint64(sizeof buffer)));
        if (n <= 0)
            return false;
        size -= n;
    }
    return true;
}

} // namespace

bool parseWave(QIODevice *device, WaveInfo *info, QString *error) {
    auto fail = [error](const char *message) {
        *error = QString::fromUtf8(message);
        return false;
    };
    char riff[12];
    if (!readExactly(device, riff, 12) || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0)
        return fail("not a WAV file");
    qint64 offset = 12;
    bool haveFormat = false;
    quint16 tag = 0;
    int channels = 0;
    int rate = 0;
    int bits = 0;
    quint32 dataSize = 0;
    for (;;) {
        char header[8];
        if (!readExactly(device, header, 8))
            return fail("the WAV file has no data");
        offset += 8;
        const quint32 size = qFromLittleEndian<quint32>(header + 4);
        if (std::memcmp(header, "fmt ", 4) == 0) {
            if (size < 16 || size > 1024)
                return fail("the WAV format chunk is malformed");
            char fmt[1024];
            if (!readExactly(device, fmt, size))
                return fail("the WAV file is truncated");
            tag = qFromLittleEndian<quint16>(fmt);
            channels = qFromLittleEndian<quint16>(fmt + 2);
            rate = int(qFromLittleEndian<quint32>(fmt + 4));
            bits = qFromLittleEndian<quint16>(fmt + 14);
            if (tag == kFormatExtensible) {
                if (size < 40)
                    return fail("the WAV format chunk is malformed");
                tag = qFromLittleEndian<quint16>(fmt + 24); // the first two bytes of the sub-format GUID
            }
            if (size % 2 && !skip(device, 1))
                return fail("the WAV file is truncated");
            offset += size + size % 2;
            haveFormat = true;
        } else if (std::memcmp(header, "data", 4) == 0) {
            if (!haveFormat)
                return fail("the WAV file has no format chunk");
            dataSize = size;
            break;
        } else {
            if (!skip(device, qint64(size) + size % 2))
                return fail("the WAV file is truncated");
            offset += qint64(size) + size % 2;
        }
    }
    QAudioFormat::SampleFormat sampleFormat = QAudioFormat::Unknown;
    bool isFloat = false;
    if (tag == kFormatPcm) {
        if (bits == 8)
            sampleFormat = QAudioFormat::UInt8;
        else if (bits == 16)
            sampleFormat = QAudioFormat::Int16;
        else if (bits == 24 || bits == 32)
            sampleFormat = QAudioFormat::Int32;
    } else if (tag == kFormatFloat) {
        isFloat = true;
        if (bits == 32 || bits == 64)
            sampleFormat = QAudioFormat::Float;
    }
    if (sampleFormat == QAudioFormat::Unknown)
        return fail("the WAV file is compressed or has an unsupported sample size");
    if (channels < 1 || channels > 8 || rate < 1000 || rate > 768000)
        return fail("the WAV file has an unsupported channel count or sample rate");

    info->format.setSampleRate(rate);
    info->format.setChannelCount(channels);
    info->format.setSampleFormat(sampleFormat);
    info->sourceBytesPerSample = bits / 8;
    info->sourceFloat = isFloat;
    info->dataOffset = offset;
    // Streaming writers leave the size at 0 or 0xFFFFFFFF; then the data runs to the end of the file.
    qint64 bytes = dataSize;
    const qint64 rest = device->isSequential() ? -1 : device->size() - device->pos();
    if (dataSize == 0 || dataSize == 0xFFFFFFFFu)
        bytes = rest >= 0 ? rest : qint64(0x7FFFFFFFFFFF);
    else if (rest >= 0)
        bytes = std::min(bytes, rest);
    info->frames = bytes / info->sourceBytesPerFrame();
    return true;
}

QByteArray waveHeader(const QAudioFormat &format, qint64 dataBytes) {
    QByteArray h(44, '\0');
    char *p = h.data();
    const auto size = quint32(std::min<qint64>(dataBytes, 0xFFFFFFFFLL - 36));
    const auto bits = quint16(format.bytesPerSample() * 8);
    std::copy_n("RIFF", 4, p);
    qToLittleEndian<quint32>(36 + size, p + 4);
    std::copy_n("WAVEfmt ", 8, p + 8);
    qToLittleEndian<quint32>(16, p + 16);
    qToLittleEndian<quint16>(format.sampleFormat() == QAudioFormat::Float ? kFormatFloat : kFormatPcm, p + 20);
    qToLittleEndian<quint16>(quint16(format.channelCount()), p + 22);
    qToLittleEndian<quint32>(quint32(format.sampleRate()), p + 24);
    qToLittleEndian<quint32>(quint32(format.sampleRate() * format.bytesPerFrame()), p + 28);
    qToLittleEndian<quint16>(quint16(format.bytesPerFrame()), p + 32);
    qToLittleEndian<quint16>(bits, p + 34);
    std::copy_n("data", 4, p + 36);
    qToLittleEndian<quint32>(size, p + 40);
    return h;
}

void convertWaveFrames(const WaveInfo &info, const char *in, char *out, qint64 frames) {
    const qint64 samples = frames * info.format.channelCount();
    const int from = info.sourceBytesPerSample;
    if (from == info.format.bytesPerSample()) {
        std::memcpy(out, in, std::size_t(samples * from));
        return;
    }
    if (from == 3) {
        for (qint64 i = 0; i < samples; ++i) {
            const auto *s = reinterpret_cast<const unsigned char *>(in + i * 3);
            const quint32 v = quint32(s[0]) << 8 | quint32(s[1]) << 16 | quint32(s[2]) << 24;
            std::memcpy(out + i * 4, &v, 4);
        }
    } else if (from == 8 && info.sourceFloat) {
        for (qint64 i = 0; i < samples; ++i) {
            double v;
            std::memcpy(&v, in + i * 8, 8);
            const auto f = float(v);
            std::memcpy(out + i * 4, &f, 4);
        }
    }
}

qint64 WaveReader::bytesAvailable() const {
    if (m_loopsLeft == 0)
        return QIODevice::bytesAvailable();
    return (m_info.frames - m_frame) * m_info.format.bytesPerFrame() + QIODevice::bytesAvailable();
}

bool WaveReader::seekFrame(qint64 frame) {
    m_frame = std::clamp<qint64>(frame, 0, m_info.frames);
    m_needSeek = true;
    return true;
}

qint64 WaveReader::readData(char *data, qint64 maxSize) {
    const int outFrame = m_info.format.bytesPerFrame();
    const int inFrame = m_info.sourceBytesPerFrame();
    if (outFrame <= 0 || inFrame <= 0)
        return -1;
    qint64 done = 0;
    while (m_loopsLeft != 0 && maxSize - done >= outFrame) {
        if (m_frame >= m_info.frames) {
            if (m_loopsLeft > 0)
                --m_loopsLeft;
            if (m_onLoop)
                m_onLoop();
            if (m_loopsLeft == 0)
                break;
            m_frame = 0;
            m_needSeek = true;
        }
        if (m_needSeek) {
            if (!m_file->isSequential() && !m_file->seek(m_info.dataOffset + m_frame * inFrame))
                return done > 0 ? done : -1;
            m_needSeek = false;
        }
        const qint64 frames = std::min((maxSize - done) / outFrame, m_info.frames - m_frame);
        char *target = data + done;
        qint64 got = 0;
        if (inFrame == outFrame) {
            got = m_file->read(target, frames * inFrame);
        } else {
            m_scratch.resize(qsizetype(frames * inFrame));
            got = m_file->read(m_scratch.data(), frames * inFrame);
        }
        if (got <= 0) {
            // The file is shorter than its header says: treat this as the end of the loop.
            m_info.frames = m_frame;
            if (m_frame == 0) {
                m_loopsLeft = 0;
                break;
            }
            continue;
        }
        const qint64 gotFrames = got / inFrame;
        if (inFrame != outFrame)
            convertWaveFrames(m_info, m_scratch.constData(), target, gotFrames);
        m_frame += gotFrames;
        done += gotFrames * outFrame;
        if (got % inFrame) {
            m_needSeek = true; // realign on the next read
        }
    }
    return done;
}

} // namespace QtMultimediaPrivate
