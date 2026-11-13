// MIT License - see LICENSE
//
// Reading WAV files for QSoundEffect and QMediaPlayer: the RIFF header, and the conversion of the
// samples to a QAudioFormat that a sink can play (24-bit samples become 32-bit, 64-bit floats
// become 32-bit).
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qaudioformat.h>

#include <functional>

namespace QtMultimediaPrivate {

struct WaveInfo {
    QAudioFormat format; // what the samples are converted to
    int sourceBytesPerSample = 0;
    bool sourceFloat = false;
    qint64 dataOffset = 0; // where the samples start in the file
    qint64 frames = 0;

    int sourceBytesPerFrame() const { return sourceBytesPerSample * format.channelCount(); }
    qint64 durationMs() const { return format.sampleRate() > 0 ? frames * 1000 / format.sampleRate() : 0; }
};

// Reads the header of the WAV file in device, from its current position, and leaves the device at
// the first sample. Returns false and sets error if it is not a WAV file Nt can play.
bool parseWave(QIODevice *device, WaveInfo *info, QString *error);

// The 44-byte header of a WAV file of dataBytes bytes of samples in format.
QByteArray waveHeader(const QAudioFormat &format, qint64 dataBytes);

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

} // namespace QtMultimediaPrivate
