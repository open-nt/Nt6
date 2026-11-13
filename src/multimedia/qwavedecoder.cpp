// MIT License - see LICENSE
#include "qwavedecoder_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qendian.h>

#include <algorithm>
#include <cmath>
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

// The WAV INFO tags Nt reads and writes, and their keys.
struct InfoTag {
    const char *id;
    QMediaMetaData::Key key;
};
constexpr InfoTag kInfoTags[] = {
    {"INAM", QMediaMetaData::Title},      {"IART", QMediaMetaData::ContributingArtist},
    {"ICMT", QMediaMetaData::Comment},    {"ISBJ", QMediaMetaData::Description},
    {"IGNR", QMediaMetaData::Genre},      {"ICRD", QMediaMetaData::Date},
    {"ILNG", QMediaMetaData::Language},   {"ICOP", QMediaMetaData::Copyright},
    {"IPRD", QMediaMetaData::AlbumTitle}, {"ITRK", QMediaMetaData::TrackNumber},
    {"IMUS", QMediaMetaData::Composer},
};

void parseInfo(const QByteArray &list, QMediaMetaData *metaData) {
    qsizetype at = 4; // after "INFO"
    while (at + 8 <= list.size()) {
        const QByteArray id = list.mid(at, 4);
        const auto size = qsizetype(qFromLittleEndian<quint32>(list.constData() + at + 4));
        at += 8;
        if (size < 0 || at + size > list.size())
            break;
        QByteArray text = list.mid(at, size);
        const qsizetype nul = text.indexOf('\0');
        if (nul >= 0)
            text.truncate(nul);
        at += size + size % 2;
        const QString value = QString::fromUtf8(text).trimmed();
        if (value.isEmpty())
            continue;
        for (const InfoTag &tag : kInfoTags) {
            if (id != tag.id)
                continue;
            if (tag.key == QMediaMetaData::Date) {
                const QDate date = QDate::fromString(value.left(10), Qt::ISODate);
                metaData->insert(tag.key, date.isValid() ? QVariant(date) : QVariant(value));
            } else if (tag.key == QMediaMetaData::TrackNumber) {
                bool ok = false;
                const int track = value.toInt(&ok);
                metaData->insert(tag.key, ok ? QVariant(track) : QVariant(value));
            } else {
                metaData->insert(tag.key, value);
            }
        }
    }
}

QByteArray infoChunk(const QMediaMetaData &metaData) {
    QByteArray body("INFO");
    for (const InfoTag &tag : kInfoTags) {
        QVariant v = metaData.value(tag.key);
        if (!v.isValid() && tag.key == QMediaMetaData::ContributingArtist)
            v = metaData.value(QMediaMetaData::Author);
        if (!v.isValid())
            continue;
        const QByteArray text =
            (v.typeId() == QMetaType::QDate ? v.toDate().toString(Qt::ISODate) : v.toString()).toUtf8() + '\0';
        char header[8];
        std::copy_n(tag.id, 4, header);
        qToLittleEndian<quint32>(quint32(text.size()), header + 4);
        body.append(header, 8).append(text);
        if (text.size() % 2)
            body.append('\0');
    }
    if (body.size() == 4)
        return QByteArray();
    char header[8];
    std::copy_n("LIST", 4, header);
    qToLittleEndian<quint32>(quint32(body.size()), header + 4);
    return QByteArray(header, 8) + body;
}

} // namespace

bool parseWave(QIODevice *device, WaveInfo *info, QString *error) {
    auto fail = [error](const char *message) {
        *error = QString::fromUtf8(message);
        return false;
    };
    info->metaData.clear();
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
        } else if (std::memcmp(header, "LIST", 4) == 0 && size >= 4 && size < (1u << 20)) {
            QByteArray list(qsizetype(size), '\0');
            if (!readExactly(device, list.data(), size) || (size % 2 && !skip(device, 1)))
                return fail("the WAV file is truncated");
            if (list.startsWith("INFO"))
                parseInfo(list, &info->metaData);
            offset += qint64(size) + size % 2;
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

QByteArray waveHeader(const QAudioFormat &format, qint64 dataBytes, const QMediaMetaData &metaData) {
    const QByteArray info = infoChunk(metaData);
    QByteArray h(44, '\0');
    char *p = h.data();
    const auto size = quint32(std::min<qint64>(dataBytes, 0xFFFFFFFFLL - 36 - info.size()));
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
    qToLittleEndian<quint32>(quint32(36 + info.size()) + size, p + 4);
    return h.left(36) + info + h.mid(36);
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

void writeNormalizedSample(const QAudioFormat &format, char *out, float value) {
    value = std::clamp(value, -1.0f, 1.0f);
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8:
        *reinterpret_cast<unsigned char *>(out) = static_cast<unsigned char>(std::lround(value * 127.0f + 128.0f));
        break;
    case QAudioFormat::Int16: {
        const auto s = static_cast<qint16>(std::lround(value * 32767.0f));
        std::memcpy(out, &s, 2);
        break;
    }
    case QAudioFormat::Int32: {
        const auto s = static_cast<qint32>(std::llround(double(value) * 2147483647.0));
        std::memcpy(out, &s, 4);
        break;
    }
    case QAudioFormat::Float:
        std::memcpy(out, &value, 4);
        break;
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        break;
    }
}

bool RateReader::fill() {
    const int frame = m_format.bytesPerFrame();
    if (frame <= 0)
        return false;
    m_chunk.resize(4096 * frame);
    const qint64 n = m_source->read(m_chunk.data(), m_chunk.size());
    if (n <= 0)
        return false;
    const qint64 samples = n / m_format.bytesPerSample();
    const int bytes = m_format.bytesPerSample();
    for (qint64 i = 0; i < samples; ++i)
        m_frames.push_back(m_format.normalizedSampleValue(m_chunk.constData() + i * bytes));
    return true;
}

bool RateReader::atEnd() const {
    const auto have = qint64(m_frames.size()) / std::max(m_format.channelCount(), 1);
    return m_source->atEnd() && m_t + 1.0 >= double(have);
}

qint64 RateReader::bytesAvailable() const {
    return QIODevice::bytesAvailable() + (atEnd() ? 0 : m_format.bytesPerFrame());
}

qint64 RateReader::readData(char *data, qint64 maxSize) {
    const int ch = m_format.channelCount();
    const int frame = m_format.bytesPerFrame();
    const int sample = m_format.bytesPerSample();
    if (ch <= 0 || frame <= 0)
        return -1;
    qint64 done = 0;
    while (maxSize - done >= frame) {
        auto have = qint64(m_frames.size()) / ch;
        if (m_t + 1.0 >= double(have)) {
            // Keep only the frame the next output needs, then read more.
            const auto keep = std::min<qint64>(qint64(m_t), have);
            m_frames.erase(m_frames.begin(), m_frames.begin() + std::ptrdiff_t(keep * ch));
            m_t -= double(keep);
            if (!fill())
                break;
            continue;
        }
        const auto i = qint64(m_t);
        const auto frac = float(m_t - double(i));
        for (int c = 0; c < ch; ++c) {
            const float a = m_frames[std::size_t(i * ch + c)];
            const float b = m_frames[std::size_t((i + 1) * ch + c)];
            writeNormalizedSample(m_format, data + done + c * sample, a + (b - a) * frac);
        }
        done += frame;
        m_t += m_rate;
    }
    return done;
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
