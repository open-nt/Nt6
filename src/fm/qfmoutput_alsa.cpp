// MIT License - see LICENSE
//
// The default sound output on Linux: ALSA's "default" device, written from a thread of its own.
// libasound is loaded when an engine starts rather than linked, so Nt FM builds without its
// headers and runs, offline, on systems without it. The few functions used are declared here;
// their ABI has been stable since ALSA 1.0.

#include "qfmoutput_p.h"

#include <QtCore/qlibrary.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <thread>
#include <vector>

namespace QtFM::Private {

namespace {

struct snd_pcm_t;
using snd_pcm_sframes_t = long;
using snd_pcm_uframes_t = unsigned long;

// Values of the ALSA enumerations, from <alsa/pcm.h>.
constexpr int kStreamPlayback = 0;
constexpr int kFormatS16Le = 2;
constexpr int kFormatFloatLe = 14;
constexpr int kAccessRwInterleaved = 3;

using OpenFn = int (*)(snd_pcm_t **, const char *, int, int);
using SetParamsFn = int (*)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
using WriteFn = snd_pcm_sframes_t (*)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
using RecoverFn = int (*)(snd_pcm_t *, int, int);
using CloseFn = int (*)(snd_pcm_t *);
using DrainFn = int (*)(snd_pcm_t *);

class AlsaOutput : public AudioOutput {
public:
    explicit AlsaOutput(int blockSize) : m_block(std::max(blockSize, 32)) {}
    ~AlsaOutput() override { AlsaOutput::close(); }

    bool open(int sampleRate, int channels, SampleFormat format) override {
        if (m_pcm || channels != 2)
            return false;
        m_library.setFileNameAndVersion(QStringLiteral("asound"), 2);
        if (!m_library.load())
            return false;
        m_open = reinterpret_cast<OpenFn>(m_library.resolve("snd_pcm_open"));
        m_setParams = reinterpret_cast<SetParamsFn>(m_library.resolve("snd_pcm_set_params"));
        m_write = reinterpret_cast<WriteFn>(m_library.resolve("snd_pcm_writei"));
        m_recover = reinterpret_cast<RecoverFn>(m_library.resolve("snd_pcm_recover"));
        m_close = reinterpret_cast<CloseFn>(m_library.resolve("snd_pcm_close"));
        m_drop = reinterpret_cast<DrainFn>(m_library.resolve("snd_pcm_drop"));
        if (!m_open || !m_setParams || !m_write || !m_recover || !m_close || !m_drop)
            return false;
        if (m_open(&m_pcm, "default", kStreamPlayback, 0) < 0) {
            m_pcm = nullptr;
            return false;
        }
        // Latency of about two blocks.
        const auto latency = static_cast<unsigned>(2LL * m_block * 1000000LL / std::max(sampleRate, 1));
        m_float = format == SampleFormat::Float32;
        int err = m_setParams(m_pcm, m_float ? kFormatFloatLe : kFormatS16Le, kAccessRwInterleaved, 2,
                              static_cast<unsigned>(sampleRate), 1, latency);
        if (err < 0 && m_float) {
            m_float = false;
            err = m_setParams(m_pcm, kFormatS16Le, kAccessRwInterleaved, 2, static_cast<unsigned>(sampleRate), 1,
                              latency);
        }
        if (err < 0) {
            m_close(m_pcm);
            m_pcm = nullptr;
            return false;
        }
        m_floats.assign(static_cast<std::size_t>(m_block) * 2, 0.0f);
        m_shorts.assign(static_cast<std::size_t>(m_block) * 2, 0);
        m_running.store(true);
        m_thread = std::thread([this] { run(); });
        return true;
    }

    void close() override {
        if (!m_pcm)
            return;
        m_running.store(false);
        if (m_thread.joinable())
            m_thread.join();
        m_drop(m_pcm);
        m_close(m_pcm);
        m_pcm = nullptr;
    }

private:
    void run() {
        const auto frames = static_cast<snd_pcm_uframes_t>(m_block);
        while (m_running.load(std::memory_order_relaxed)) {
            const void *data;
            if (m_float) {
                pull(m_floats.data(), m_block);
                data = m_floats.data();
            } else {
                pull(m_shorts.data(), m_block);
                data = m_shorts.data();
            }
            snd_pcm_sframes_t written = m_write(m_pcm, data, frames);
            if (written == -EPIPE || written == -ESTRPIPE || written == -EINTR) {
                if (written == -EPIPE)
                    reportUnderrun();
                if (m_recover(m_pcm, static_cast<int>(written), 1) < 0)
                    break;
            } else if (written == -EAGAIN || written == -EWOULDBLOCK) {
                continue;
            } else if (written < 0) {
                break;
            }
        }
    }

    int m_block;
    QLibrary m_library;
    snd_pcm_t *m_pcm = nullptr;
    bool m_float = true;
    OpenFn m_open = nullptr;
    SetParamsFn m_setParams = nullptr;
    WriteFn m_write = nullptr;
    RecoverFn m_recover = nullptr;
    CloseFn m_close = nullptr;
    DrainFn m_drop = nullptr;
    std::vector<float> m_floats;
    std::vector<qint16> m_shorts;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace

std::unique_ptr<AudioOutput> createDefaultAudioOutput(int blockSize) { return std::make_unique<AlsaOutput>(blockSize); }

} // namespace QtFM::Private
