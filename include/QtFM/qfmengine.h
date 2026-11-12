// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtFM/qfmaudiooutput.h>
#include <QtFM/qfmglobal.h>
#include <QtFM/qfmpatch.h>

#include <memory>

namespace QtFM {

namespace Private {
struct EngineCore;
struct Program;
} // namespace Private

struct EngineConfig {
    int sampleRate = 48000;
    int maxVoices = 32;
    int blockSize = 256;
    SampleFormat format = SampleFormat::Float32;
    quint32 seed = 0;
    int queueCapacity = 1024;
};

struct PlayOptions {
    float pitch = 0;
    float gain = 0;
    float pan = 0;
    float duration = -1;
    float delay = 0;
    int priority = 0;
    quint32 seed = 0;
    int bus = 0;
};

class VoiceHandle {
public:
    VoiceHandle();
    VoiceHandle(const VoiceHandle &other);
    VoiceHandle &operator=(const VoiceHandle &other);
    ~VoiceHandle();

    bool isValid() const;
    quint32 id() const;
    bool isPlaying() const;
    void stop(bool immediate = false);
    void setPitch(float semitones);
    void setGain(float dB);
    void setPan(float pan);
    void setParameter(ParamId id, float value);

private:
    friend class Engine;
    VoiceHandle(std::shared_ptr<Private::Program> program, std::weak_ptr<Private::EngineCore> core);
    std::shared_ptr<Private::Program> m_program;
    std::weak_ptr<Private::EngineCore> m_core;
};

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(const EngineConfig &config = {}, QObject *parent = nullptr);
    ~Engine() override;

    EngineConfig config() const;

    bool start(AudioOutput *output = nullptr);
    void stop();
    bool isStarted() const;

    VoiceHandle play(const Patch &patch, const PlayOptions &options = {});
    void stopAll(bool immediate = false);

    int render(float *interleaved, int frames);
    int render(qint16 *interleaved, int frames);

    void setBusGain(int bus, float dB);
    float busGain(int bus) const;
    void setMasterGain(float dB);
    float masterGain() const;
    int activeVoices() const;

    static QByteArray renderToWav(const Patch &patch, int sampleRate = 48000, const PlayOptions &options = {});

signals:
    void voiceFinished(quint32 voiceId);
    void underrun();

private:
    friend class AudioOutput;
    void dispatchEvents();
    std::shared_ptr<Private::EngineCore> m_core;
    AudioOutput *m_output = nullptr;
    std::unique_ptr<AudioOutput> m_ownedOutput;
};

QByteArray renderToWav(const Patch &patch, int sampleRate = 48000, const PlayOptions &options = {});

} // namespace QtFM
