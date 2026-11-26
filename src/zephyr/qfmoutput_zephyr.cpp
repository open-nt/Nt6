// MIT License - see LICENSE
//
// The default sound output of Nt FM on Zephyr: the audio codec that Nt Multimedia drives (the WM8962 on SAI1 of the
// MIMXRT1170-EVKB, see qaudiobackend_zephyr.cpp), opened through its private backend in stereo with 16-bit samples.
// A thread of its own renders a block at a time and writes it to the codec's stream; the write blocks while the
// stream's ring is full, which paces the rendering at the codec's rate. Zephyr's C++ library has no std::thread, so
// the thread is detached and close() waits for it to say that it has finished.

#include "qfmoutput_p.h"

#include "qaudiobackend_p.h"
#include "threaddata_p.h"

#include <QtCore/qlogging.h>

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace QtFM::Private {

namespace {

class ZephyrCodecOutput : public AudioOutput {
public:
    explicit ZephyrCodecOutput(int blockSize) : m_block(std::max(blockSize, 32)) {}
    ~ZephyrCodecOutput() override { ZephyrCodecOutput::close(); }
    ZephyrCodecOutput(const ZephyrCodecOutput &) = delete;
    ZephyrCodecOutput &operator=(const ZephyrCodecOutput &) = delete;

    bool open(int sampleRate, int channels, SampleFormat format) override {
        // The codec takes integer samples; the engine converts its floats when it is pulled as 16-bit.
        Q_UNUSED(format);
        if (m_pcm || channels != 2)
            return false;
        QAudioFormat audioFormat;
        audioFormat.setSampleRate(sampleRate);
        audioFormat.setChannelCount(2);
        audioFormat.setSampleFormat(QAudioFormat::Int16);
        const qint64 periodBytes = qint64(m_block) * audioFormat.bytesPerFrame();
        QString error;
        m_pcm = QtMultimediaPrivate::audioBackend()->open(QByteArray(), QAudioDevice::Output, audioFormat,
                                                          periodBytes, &error);
        if (!m_pcm) {
            qWarning("QtFM::Engine: cannot open the audio codec: %s", qPrintable(error));
            return false;
        }
        m_samples.assign(static_cast<std::size_t>(m_block) * 2, 0);
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = false;
            m_running = true;
        }
        if (!QtPrivate::startDetachedThread([this] { run(); })) {
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                m_running = false;
            }
            m_pcm.reset();
            return false;
        }
        return true;
    }

    void close() override {
        if (!m_pcm)
            return;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stop = true;
            m_finished.wait(lock, [this] { return !m_running; });
        }
        m_pcm->drop();
        m_pcm.reset();
    }

private:
    void run() {
        const qint64 bytes = qint64(m_samples.size() * sizeof(qint16));
        for (;;) {
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stop)
                    break;
            }
            pull(m_samples.data(), m_block);
            // -1 when the codec's stream has failed, after the backend has tried to restart it.
            if (m_pcm->write(reinterpret_cast<const char *>(m_samples.data()), bytes) < 0)
                break;
        }
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        // Notified under the lock, so that close() cannot destroy the output before this is done.
        m_finished.notify_all();
    }

    int m_block;
    std::unique_ptr<QtMultimediaPrivate::AudioPcm> m_pcm;
    std::vector<qint16> m_samples;
    std::mutex m_mutex;
    std::condition_variable m_finished;
    bool m_running = false;
    bool m_stop = false;
};

} // namespace

std::unique_ptr<AudioOutput> createDefaultAudioOutput(int blockSize) {
    return std::make_unique<ZephyrCodecOutput>(blockSize);
}

} // namespace QtFM::Private
