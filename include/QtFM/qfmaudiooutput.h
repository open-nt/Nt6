// MIT License - see LICENSE
#pragma once

#include <QtFM/qfmglobal.h>

#include <atomic>

namespace QtFM {

class Engine;

class AudioOutput {
public:
    AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;
    virtual ~AudioOutput();

    virtual bool open(int sampleRate, int channels, SampleFormat format) = 0;
    virtual void close() = 0;

protected:
    int pull(float *interleaved, int frames);
    int pull(qint16 *interleaved, int frames);
    void reportUnderrun();

private:
    friend class Engine;
    std::atomic<Engine *> m_engine{nullptr};
};

} // namespace QtFM
