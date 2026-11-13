// MIT License - see LICENSE
//
// The platform part of Nt Multimedia.
//
// An AudioBackend lists the sound devices and opens PCM streams on them; qaudiobackend_alsa.cpp
// does this with ALSA, loaded at run time, and qaudiobackend_null.cpp has no devices. An open
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
};

// The backend in use, created on first use.
AudioBackend *audioBackend();
// Replaces the backend; for tests. Devices and streams of the old one must be gone.
void setAudioBackend(std::unique_ptr<AudioBackend> backend);
// The platform's backend.
std::unique_ptr<AudioBackend> createPlatformAudioBackend();

// Makes a device for a backend to return.
QAudioDevice makeAudioDevice(const QAudioDevicePrivate &info);

// Scales the samples of a buffer in format by volume (0.0 to 1.0, or more for gain), in place.
void applyVolume(char *data, qint64 bytes, QAudioFormat::SampleFormat format, float volume);

} // namespace QtMultimediaPrivate
