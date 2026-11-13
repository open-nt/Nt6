// MIT License - see LICENSE
//
// Reading WAV files for QSoundEffect and QMediaPlayer: the RIFF header, and the conversion of the
// samples to a QAudioFormat that a sink can play (24-bit samples become 32-bit, 64-bit floats
// become 32-bit).
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qmediametadata.h>

#include <functional>
#include <vector>

namespace QtMultimediaPrivate {

struct WaveInfo {
    QAudioFormat format; // what the samples are converted to
    int sourceBytesPerSample = 0;
    bool sourceFloat = false;
    qint64 dataOffset = 0; // where the samples start in the file
    qint64 frames = 0;
    QMediaMetaData metaData; // from the LIST/INFO chunk before the samples

    int sourceBytesPerFrame() const { return sourceBytesPerSample * format.channelCount(); }
    qint64 durationMs() const { return format.sampleRate() > 0 ? frames * 1000 / format.sampleRate() : 0; }
};

// Reads the header of the WAV file in device, from its current position, and leaves the device at
// the first sample. Returns false and sets error if it is not a WAV file Nt can play.
bool parseWave(QIODevice *device, WaveInfo *info, QString *error);

// The header of a WAV file of dataBytes bytes of samples in format, with the text tags of metaData
// in a LIST/INFO chunk: 44 bytes without tags.
QByteArray waveHeader(const QAudioFormat &format, qint64 dataBytes, const QMediaMetaData &metaData = {});

// Converts frames frames from the file's layout at in to info.format at out.
void convertWaveFrames(const WaveInfo &info, const char *in, char *out, qint64 frames);

// Reads frames from the samples of a WAV file, converted, looping a number of times; the device a
// sink pulls from. loops is the number of times to play, or a negative number for ever.
class WaveReader : public QIODevice {
public:
    WaveReader(QIODevice *file, const WaveInfo &info) : m_file(file), m_info(info) { setOpenMode(ReadOnly); }

    void setLoops(int loops) { m_loopsLeft = loops; }
    int loopsLeft() const { return m_loopsLeft; }
    // Moves to frame, for the next read.
    bool seekFrame(qint64 frame);
    qint64 frame() const { return m_frame; }
    // Called when a loop has been read to its end.
    void setLoopCallback(std::function<void()> callback) { m_onLoop = std::move(callback); }

    bool isSequential() const override { return true; }
    bool atEnd() const override { return m_loopsLeft == 0 || (m_loopsLeft == 1 && m_frame >= m_info.frames); }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QIODevice *m_file;
    WaveInfo m_info;
    qint64 m_frame = 0;
    int m_loopsLeft = 1;
    bool m_needSeek = true;
    QByteArray m_scratch;
    std::function<void()> m_onLoop;
};

// Writes value, from -1.0 to 1.0, as one sample in format at out.
void writeNormalizedSample(const QAudioFormat &format, char *out, float value);

// Plays what a source reads at rate times its speed, by linear interpolation, so the pitch changes
// with the speed; the device a sink pulls from when the playback rate is not 1.
class RateReader : public QIODevice {
public:
    RateReader(QIODevice *source, const QAudioFormat &format, double rate)
        : m_source(source), m_format(format), m_rate(rate) {
        setOpenMode(ReadOnly);
    }

    bool isSequential() const override { return true; }
    bool atEnd() const override;
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    // Reads more of the source into m_frames; returns false at its end.
    bool fill();

    QIODevice *m_source;
    QAudioFormat m_format;
    double m_rate;
    std::vector<float> m_frames; // the source as samples, from frame m_base on
    double m_t = 0.0;            // the next output frame, in source frames after m_base
    QByteArray m_chunk;
};

} // namespace QtMultimediaPrivate
