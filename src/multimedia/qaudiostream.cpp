// MIT License - see LICENSE
#include "qaudiostream_p.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace QtMultimediaPrivate {

namespace {

std::mutex &backendMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unique_ptr<AudioBackend> &backendInstance() {
    static std::unique_ptr<AudioBackend> backend;
    return backend;
}

template <typename T> void scale(char *data, qint64 bytes, float volume, double low, double high) {
    const qint64 count = bytes / qint64(sizeof(T));
    for (qint64 i = 0; i < count; ++i) {
        T sample;
        std::memcpy(&sample, data + i * qint64(sizeof(T)), sizeof(T));
        const double v = std::clamp(std::round(double(sample) * double(volume)), low, high);
        sample = static_cast<T>(v);
        std::memcpy(data + i * qint64(sizeof(T)), &sample, sizeof(T));
    }
}

} // namespace

AudioBackend *audioBackend() {
    const std::lock_guard<std::mutex> lock(backendMutex());
    auto &backend = backendInstance();
    if (!backend)
        backend = createPlatformAudioBackend();
    return backend.get();
}

void setAudioBackend(std::unique_ptr<AudioBackend> backend) {
    const std::lock_guard<std::mutex> lock(backendMutex());
    backendInstance() = std::move(backend);
}

QAudioDevice makeAudioDevice(const QAudioDevicePrivate &info) { return QAudioDevice(new QAudioDevicePrivate(info)); }

void applyVolume(char *data, qint64 bytes, QAudioFormat::SampleFormat format, float volume) {
    if (volume == 1.0f)
        return;
    switch (format) {
    case QAudioFormat::UInt8: {
        auto *p = reinterpret_cast<unsigned char *>(data);
        for (qint64 i = 0; i < bytes; ++i) {
            const double v = std::round((double(p[i]) - 128.0) * double(volume)) + 128.0;
            p[i] = static_cast<unsigned char>(std::clamp(v, 0.0, 255.0));
        }
        break;
    }
    case QAudioFormat::Int16:
        scale<qint16>(data, bytes, volume, -32768.0, 32767.0);
        break;
    case QAudioFormat::Int32:
        scale<qint32>(data, bytes, volume, -2147483648.0, 2147483647.0);
        break;
    case QAudioFormat::Float: {
        const qint64 count = bytes / qint64(sizeof(float));
        for (qint64 i = 0; i < count; ++i) {
            float sample;
            std::memcpy(&sample, data + i * qint64(sizeof(float)), sizeof(float));
            sample *= volume;
            std::memcpy(data + i * qint64(sizeof(float)), &sample, sizeof(float));
        }
        break;
    }
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        break;
    }
}

AudioStream::AudioStream(QAudioDevice::Mode mode, const QAudioFormat &format, qint64 bufferBytes)
    : m_mode(mode), m_format(format) {
    const qint64 frame = std::max<qint64>(format.bytesPerFrame(), 1);
    bufferBytes = std::max(bufferBytes / frame, qint64(4)) * frame;
    m_period = std::max(bufferBytes / 4 / frame, qint64(1)) * frame;
    m_ring.resize(std::size_t(bufferBytes));
}

AudioStream::~AudioStream() { close(false); }

qint64 AudioStream::frameAligned(qint64 bytes) const {
    const qint64 frame = std::max<qint64>(m_format.bytesPerFrame(), 1);
    return bytes / frame * frame;
}

bool AudioStream::open(const QAudioDevice &device, QString *error) {
    if (m_pcm)
        return true;
    AudioBackend *backend = audioBackend();
    if (!backend) {
        *error = QStringLiteral("no audio backend");
        return false;
    }
    m_pcm = backend->open(device.id(), m_mode, m_format, m_period, error);
    if (!m_pcm)
        return false;
    m_stop = false;
    m_failed.store(false);
    m_processed.store(0);
    m_thread = std::thread([this] {
        if (m_mode == QAudioDevice::Output)
            runOutput();
        else
            runInput();
    });
    return true;
}

void AudioStream::close(bool drain) {
    if (!m_pcm)
        return;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    if (drain && m_mode == QAudioDevice::Output && !m_failed.load()) {
        // Let the device play out what it holds without holding up the caller.
        std::thread([pcm = std::shared_ptr<AudioPcm>(std::move(m_pcm))] { pcm->drain(); }).detach();
    } else {
        m_pcm->drop();
        m_pcm.reset();
    }
    clear();
}

qint64 AudioStream::write(const char *data, qint64 bytes) {
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto size = qint64(m_ring.size());
    const qint64 n = frameAligned(std::min(bytes, size - m_count));
    if (n <= 0)
        return 0;
    const qint64 tail = (m_head + m_count) % size;
    const qint64 first = std::min(n, size - tail);
    std::memcpy(m_ring.data() + tail, data, std::size_t(first));
    if (n > first)
        std::memcpy(m_ring.data(), data + first, std::size_t(n - first));
    m_count += n;
    lock.unlock();
    m_wake.notify_all();
    return n;
}

qint64 AudioStream::freeBytes() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return frameAligned(qint64(m_ring.size()) - m_count);
}

bool AudioStream::starved() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_count == 0 && m_inFlight == 0;
}

qint64 AudioStream::read(char *data, qint64 bytes) {
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto size = qint64(m_ring.size());
    const qint64 n = frameAligned(std::min(bytes, m_count));
    if (n <= 0)
        return 0;
    const qint64 first = std::min(n, size - m_head);
    std::memcpy(data, m_ring.data() + m_head, std::size_t(first));
    if (n > first)
        std::memcpy(data + first, m_ring.data(), std::size_t(n - first));
    m_head = (m_head + n) % size;
    m_count -= n;
    return n;
}

qint64 AudioStream::availableBytes() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
}

void AudioStream::clear() {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_head = 0;
    m_count = 0;
}

void AudioStream::setSuspended(bool suspended) {
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_suspended = suspended;
    }
    m_wake.notify_all();
}

void AudioStream::runOutput() {
    std::vector<char> chunk(static_cast<std::size_t>(m_period));
    bool paused = false;
    const auto size = qint64(m_ring.size());
    for (;;) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wake.wait(lock, [&] { return m_stop || m_suspended != paused || (!m_suspended && m_count > 0); });
        if (m_stop)
            return;
        if (m_suspended != paused) {
            paused = m_suspended;
            lock.unlock();
            m_pcm->setPaused(paused);
            continue;
        }
        const qint64 n = std::min(m_count, m_period);
        const qint64 first = std::min(n, size - m_head);
        std::memcpy(chunk.data(), m_ring.data() + m_head, std::size_t(first));
        if (n > first)
            std::memcpy(chunk.data() + first, m_ring.data(), std::size_t(n - first));
        m_head = (m_head + n) % size;
        m_count -= n;
        m_inFlight = n;
        lock.unlock();

        applyVolume(chunk.data(), n, m_format.sampleFormat(), m_volume.load());
        const qint64 written = m_pcm->write(chunk.data(), n);

        lock.lock();
        m_inFlight = 0;
        if (written < 0) {
            m_failed.store(true);
            return;
        }
        m_processed.fetch_add(n);
    }
}

void AudioStream::runInput() {
    std::vector<char> chunk(static_cast<std::size_t>(m_period));
    bool paused = false;
    const auto size = qint64(m_ring.size());
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [&] { return m_stop || !m_suspended || !paused; });
            if (m_stop)
                return;
            if (m_suspended != paused) {
                paused = m_suspended;
                lock.unlock();
                m_pcm->setPaused(paused);
                continue;
            }
        }
        const qint64 got = m_pcm->read(chunk.data(), m_period);
        if (got < 0) {
            m_failed.store(true);
            return;
        }
        const qint64 n = frameAligned(got);
        applyVolume(chunk.data(), n, m_format.sampleFormat(), m_volume.load());
        const std::lock_guard<std::mutex> lock(m_mutex);
        // What does not fit is lost, as when a device overruns.
        const qint64 take = std::min(n, size - m_count);
        const qint64 tail = (m_head + m_count) % size;
        const qint64 first = std::min(take, size - tail);
        std::memcpy(m_ring.data() + tail, chunk.data(), std::size_t(first));
        if (take > first)
            std::memcpy(m_ring.data(), chunk.data() + first, std::size_t(take - first));
        m_count += take;
        m_processed.fetch_add(n);
    }
}

} // namespace QtMultimediaPrivate
