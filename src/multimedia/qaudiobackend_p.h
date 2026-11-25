// MIT License - see LICENSE
//
// The platform part of Nt Multimedia.
//
// An AudioBackend lists the sound devices and opens PCM streams on them; qaudiobackend_alsa.cpp
// does this with ALSA, loaded at run time, src/zephyr/qaudiobackend_zephyr.cpp with the I2S and
// audio codec drivers of Zephyr, and qaudiobackend_null.cpp has no devices. An open
// AudioPcm is driven from one worker thread only (see qaudiostream_p.h), with blocking reads and
// writes of whole frames. The tests replace the backend with setAudioBackend().
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qaudiodevice.h>
#include <QtMultimedia/qaudioformat.h>

#include <memory>
#include <vector>

class QAudioDevicePrivate : public QSharedData {
public:
    QByteArray id;
    QString description;
    bool isDefault = false;
    QAudioDevice::Mode mode = QAudioDevice::Null;
    QAudioFormat preferredFormat;
    int minimumSampleRate = 8000;
    int maximumSampleRate = 192000;
    int minimumChannelCount = 1;
    int maximumChannelCount = 8;
    QList<QAudioFormat::SampleFormat> sampleFormats{QAudioFormat::UInt8, QAudioFormat::Int16, QAudioFormat::Int32,
                                                    QAudioFormat::Float};
    QAudioFormat::ChannelConfig channelConfiguration = QAudioFormat::ChannelConfigStereo;
};

namespace QtMultimediaPrivate {

class AudioPcm {
public:
    AudioPcm() = default;
    virtual ~AudioPcm() = default;
    AudioPcm(const AudioPcm &) = delete;
    AudioPcm &operator=(const AudioPcm &) = delete;

    // Plays bytes (whole frames) of data, blocking while the device's buffer is full. Returns the
    // number of bytes taken, or -1 when the device has failed.
    virtual qint64 write(const char *data, qint64 bytes) = 0;
    // Records up to bytes (whole frames) into data, blocking until some are there. Returns the
    // number of bytes read, or -1 when the device has failed.
    virtual qint64 read(char *data, qint64 bytes) = 0;
    // Pauses or resumes the device, keeping what it has buffered if it can.
    virtual void setPaused(bool paused) { (void)paused; }
    // Throws away what the device has buffered.
    virtual void drop() {}
    // Blocks until what the device has buffered has played.
    virtual void drain() {}
};

// The settings of a headset microphone, applied by a backend's AudioHeadsetControl. See QAudioHeadset.
struct AudioHeadsetSettings {
    int bias = 2;       // QAudioHeadset::MicrophoneBias
    int channel = 1;    // 0 left, 1 right: the codec channel that carries the microphone
    int input = 3;      // the codec input pin, 1 to 4
    double gain = 24.0; // dB, from the pin to the ADC
    bool operator==(const AudioHeadsetSettings &o) const {
        return bias == o.bias && channel == o.channel && input == o.input && gain == o.gain;
    }
    bool operator!=(const AudioHeadsetSettings &o) const { return !(*this == o); }
};

// The microphone side of a codec that a headset plugs into: bias, input pin and gain, and what the codec senses
// on the microphone contact. Called from the thread of the QAudioHeadset objects; implementations lock themselves.
class AudioHeadsetControl {
public:
    enum Status { MicrophonePresent = 1, ButtonPressed = 2 };

    AudioHeadsetControl() = default;
    virtual ~AudioHeadsetControl() = default;
    AudioHeadsetControl(const AudioHeadsetControl &) = delete;
    AudioHeadsetControl &operator=(const AudioHeadsetControl &) = delete;

    // A name for the codec, such as "WM8962".
    virtual QString codecName() const = 0;
    // The lowest and highest gain, in dB, that apply() can set.
    virtual double minimumGain() const = 0;
    virtual double maximumGain() const = 0;
    // Applies the settings (now, and again whenever the codec is set up afresh). Returns false and sets error when
    // the codec cannot be reached.
    virtual bool apply(const AudioHeadsetSettings &settings, QString *error) = 0;
    // The settings last applied.
    virtual AudioHeadsetSettings settings() const = 0;
    // A combination of Status flags, or -1 when the codec cannot be read.
    virtual int status() = 0;
};

class AudioBackend {
public:
    AudioBackend() = default;
    virtual ~AudioBackend() = default;
    AudioBackend(const AudioBackend &) = delete;
    AudioBackend &operator=(const AudioBackend &) = delete;

    // The devices of mode, the default one first.
    virtual QList<QAudioDevice> devices(QAudioDevice::Mode mode) = 0;
    // Opens a stream on the device id in format; periodBytes is the size of the chunks the worker
    // thread will read or write. Returns nullptr and sets error on failure.
    virtual std::unique_ptr<AudioPcm> open(const QByteArray &id, QAudioDevice::Mode mode, const QAudioFormat &format,
                                           qint64 periodBytes, QString *error) = 0;
    // A directory whose entries change when devices come and go, or an empty string.
    virtual QString deviceDirectory() const { return QString(); }
    // The headset microphone control of the codec, or nullptr when the platform has none.
    virtual AudioHeadsetControl *headsetControl() { return nullptr; }
};

// The backend in use, created on first use.
AudioBackend *audioBackend();
// Replaces the backend; for tests. Devices and streams of the old one must be gone.
void setAudioBackend(std::unique_ptr<AudioBackend> backend);
// The platform's backend.
std::unique_ptr<AudioBackend> createPlatformAudioBackend();

// Makes a device for a backend to return.
QAudioDevice makeAudioDevice(const QAudioDevicePrivate &info);

// Splits a gain between two stages of a codec: a coarse one (a boost) and a fine one (a PGA or capture volume), each
// given as the gains, in dB, of its steps. Picks the pair whose sum is closest to target and, of equally close pairs,
// the one with the least coarse gain. An empty stage counts as one step of 0 dB. Returns the gain reached.
double splitGain(const std::vector<double> &coarse, const std::vector<double> &fine, double target,
                 std::size_t *coarseIndex, std::size_t *fineIndex);

// Scales the samples of a buffer in format by volume (0.0 to 1.0, or more for gain), in place.
void applyVolume(char *data, qint64 bytes, QAudioFormat::SampleFormat format, float volume);

} // namespace QtMultimediaPrivate
