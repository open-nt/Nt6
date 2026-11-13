// MIT License - see LICENSE
//
// The engine under QAudioSink and QAudioSource: a ring buffer between the owner's thread and a
// worker thread that does the blocking reads or writes of an AudioPcm.
//
// For output the owner write()s into the ring and the worker plays it; for input the worker records
// into the ring and the owner read()s it. The owner polls (QAudioSink and QAudioSource tick from a
// QTimer), so the worker never calls into Qt and needs no event loop.
#pragma once

#include "qaudiobackend_p.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace QtMultimediaPrivate {

class AudioStream {
public:
    AudioStream(QAudioDevice::Mode mode, const QAudioFormat &format, qint64 bufferBytes);
    ~AudioStream();
    AudioStream(const AudioStream &) = delete;
    AudioStream &operator=(const AudioStream &) = delete;

    // Opens the device and starts the worker. Returns false and sets error on failure.
    bool open(const QAudioDevice &device, QString *error);
    // Stops the worker and closes the device. With drain, output that the device holds still plays,
    // on a thread of its own, so this returns at once; otherwise it is thrown away.
    void close(bool drain);

    qint64 bufferBytes() const { return qint64(m_ring.size()); }
    qint64 periodBytes() const { return m_period; }

    // Output: queues up to bytes of data, in whole frames; returns how many were taken.
    qint64 write(const char *data, qint64 bytes);
    // Output: the room left in the ring, in whole frames.
    qint64 freeBytes() const;
    // Output: true when everything queued has been handed to the device.
    bool starved() const;

    // Input: takes up to bytes of what was recorded, in whole frames.
    qint64 read(char *data, qint64 bytes);
    // Input: how much has been recorded and not read.
    qint64 availableBytes() const;

    // Throws away what is in the ring.
    void clear();

    void setSuspended(bool suspended);
    void setVolume(float volume) { m_volume.store(volume); }

    // The bytes played or recorded since open().
    qint64 processedBytes() const { return m_processed.load(); }
    // True once the device has failed; the worker has then stopped.
    bool failed() const { return m_failed.load(); }

private:
    void runOutput();
    void runInput();
    qint64 frameAligned(qint64 bytes) const;

    QAudioDevice::Mode m_mode;
    QAudioFormat m_format;
    qint64 m_period = 0;
    std::unique_ptr<AudioPcm> m_pcm;

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::vector<char> m_ring;
    qint64 m_head = 0;  // where the oldest byte is
    qint64 m_count = 0; // bytes in the ring
    qint64 m_inFlight = 0;
    bool m_stop = false;
    bool m_suspended = false;

    std::atomic<float> m_volume{1.0f};
    std::atomic<qint64> m_processed{0};
    std::atomic<bool> m_failed{false};
    std::thread m_thread;
};

} // namespace QtMultimediaPrivate
